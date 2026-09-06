#include "RadiantTest.h"

#include "map/format/doom/DoomBinaryLoader.h"
#include "map/format/doom/DoomBrushBuilder.h"
#include "map/format/doom/DoomLightPlacer.h"
#include "map/format/doom/DoomThingTable.h"
#include "map/format/doom/SectorGeometry.h"
#include "map/format/doom/UdmfLoader.h"
#include "map/format/doom/WadFile.h"

#include "ibrush.h"
#include "imapformat.h"
#include "math/AABB.h"

#include <cmath>
#include <sstream>

namespace test
{

using namespace map::doom;

namespace
{

void appendInt32(std::string& target, int32_t value)
{
	auto raw = static_cast<uint32_t>(value);

	target.push_back(static_cast<char>(raw & 0xFF));
	target.push_back(static_cast<char>((raw >> 8) & 0xFF));
	target.push_back(static_cast<char>((raw >> 16) & 0xFF));
	target.push_back(static_cast<char>((raw >> 24) & 0xFF));
}

void appendInt16(std::string& target, int value)
{
	auto raw = static_cast<uint16_t>(value);

	target.push_back(static_cast<char>(raw & 0xFF));
	target.push_back(static_cast<char>((raw >> 8) & 0xFF));
}

void appendName(std::string& target, const std::string& name)
{
	for (std::size_t i = 0; i < 8; ++i)
	{
		target.push_back(i < name.size() ? name[i] : '\0');
	}
}

std::string buildWad(const std::vector<std::pair<std::string, std::string>>& lumps)
{
	std::string contents = "PWAD";

	appendInt32(contents, static_cast<int32_t>(lumps.size()));

	std::string directory;
	std::string payload;

	auto directoryOffset = 12;

	for (const auto& lump : lumps)
	{
		payload += lump.second;
	}

	directoryOffset += static_cast<int>(payload.size());

	appendInt32(contents, directoryOffset);

	int offset = 12;

	for (const auto& lump : lumps)
	{
		appendInt32(directory, offset);
		appendInt32(directory, static_cast<int32_t>(lump.second.size()));
		appendName(directory, lump.first);

		offset += static_cast<int>(lump.second.size());
	}

	return contents + payload + directory;
}

struct BinaryMapBuilder
{
	std::string vertexes;
	std::string sectors;
	std::string sideDefs;
	std::string lineDefs;
	std::string things;

	void addVertex(int x, int y)
	{
		appendInt16(vertexes, x);
		appendInt16(vertexes, y);
	}

	void addSector(int floor, int ceiling, int light = 160)
	{
		appendInt16(sectors, floor);
		appendInt16(sectors, ceiling);
		appendName(sectors, "FLOOR1");
		appendName(sectors, "CEIL1");
		appendInt16(sectors, light);
		appendInt16(sectors, 0);
		appendInt16(sectors, 0);
	}

	void addSideDef(int sector, const std::string& upper = "-",
		const std::string& lower = "-", const std::string& middle = "WALL1")
	{
		appendInt16(sideDefs, 0);
		appendInt16(sideDefs, 0);
		appendName(sideDefs, upper);
		appendName(sideDefs, lower);
		appendName(sideDefs, middle);
		appendInt16(sideDefs, sector);
	}

	void addLineDef(int v1, int v2, int front, int back)
	{
		appendInt16(lineDefs, v1);
		appendInt16(lineDefs, v2);
		appendInt16(lineDefs, back >= 0 ? 4 : 0);
		appendInt16(lineDefs, 0);
		appendInt16(lineDefs, 0);
		appendInt16(lineDefs, front);
		appendInt16(lineDefs, back >= 0 ? back : 0xFFFF);
	}

	void addThing(int x, int y, int angle, int type)
	{
		appendInt16(things, x);
		appendInt16(things, y);
		appendInt16(things, angle);
		appendInt16(things, type);
		appendInt16(things, 7);
	}

	std::string toWad() const
	{
		return buildWad({
			{ "MAP01", "" },
			{ "THINGS", things },
			{ "LINEDEFS", lineDefs },
			{ "SIDEDEFS", sideDefs },
			{ "VERTEXES", vertexes },
			{ "SECTORS", sectors },
		});
	}
};

BinaryMapBuilder buildSquareRoom(int size = 256, int floor = 0, int ceiling = 128)
{
	BinaryMapBuilder builder;

	builder.addVertex(0, 0);
	builder.addVertex(0, size);
	builder.addVertex(size, size);
	builder.addVertex(size, 0);

	builder.addSector(floor, ceiling);

	for (int i = 0; i < 4; ++i)
	{
		builder.addSideDef(0);
		builder.addLineDef(i, (i + 1) % 4, i, -1);
	}

	return builder;
}

BinaryMapBuilder buildRoomRow(int count, int size, int stride, const std::vector<int>& lightLevels)
{
	BinaryMapBuilder builder;

	for (int room = 0; room < count; ++room)
	{
		int x = room * stride;

		builder.addVertex(x, 0);
		builder.addVertex(x, size);
		builder.addVertex(x + size, size);
		builder.addVertex(x + size, 0);

		builder.addSector(0, 128, lightLevels[room % lightLevels.size()]);

		for (int i = 0; i < 4; ++i)
		{
			int base = room * 4;
			builder.addSideDef(room);
			builder.addLineDef(base + i, base + (i + 1) % 4, room * 4 + i, -1);
		}
	}

	return builder;
}

DoomMapData loadBinary(const std::string& wadContents)
{
	std::istringstream stream(wadContents);

	WadFile wad(stream);

	EXPECT_EQ(wad.getMaps().size(), 1u);

	return DoomBinaryLoader::load(wad, wad.getMaps().front());
}

double totalRingArea(const SectorFootprint& footprint)
{
	double area = 0;

	for (const auto& ring : footprint.rings)
	{
		area += polygon::ringArea(ring);
	}

	return area;
}

double totalPieceArea(const SectorFootprint& footprint)
{
	double area = 0;

	for (const auto& piece : footprint.pieces)
	{
		area += std::fabs(polygon::ringArea(piece));
	}

	return area;
}

}

using DoomWadTest = RadiantTest;

TEST_F(DoomWadTest, NamesThingTypes)
{
	EXPECT_EQ(getThingClassName(1), "doom_player_1_start");
	EXPECT_EQ(getThingClassName(11), "doom_deathmatch_start");
	EXPECT_EQ(getThingClassName(2035), "doom_barrel");

	EXPECT_EQ(getThingClassName(31337), "doom_thing_31337");
}

TEST_F(DoomWadTest, RecognisesWadSignature)
{
	std::istringstream wad(buildSquareRoom().toWad());
	EXPECT_TRUE(WadFile::hasWadSignature(wad));

	std::istringstream notAWad("this is not a wad file at all");
	EXPECT_FALSE(WadFile::hasWadSignature(notAWad));
}

TEST_F(DoomWadTest, FindsBinaryMapGroup)
{
	std::istringstream stream(buildSquareRoom().toWad());

	WadFile wad(stream);

	ASSERT_EQ(wad.getMaps().size(), 1u);
	EXPECT_EQ(wad.getMaps().front().name, "MAP01");
	EXPECT_EQ(wad.getMaps().front().kind, MapLumpKind::DoomBinary);
	EXPECT_TRUE(wad.findMap("map01") != nullptr);
	EXPECT_TRUE(wad.findMap("MAP02") == nullptr);
}

TEST_F(DoomWadTest, ReadsBinaryLumps)
{
	auto builder = buildSquareRoom(256, 8, 136);
	builder.addThing(64, 96, 90, 2035);

	auto data = loadBinary(builder.toWad());

	ASSERT_EQ(data.vertices.size(), 4u);
	ASSERT_EQ(data.sectors.size(), 1u);
	ASSERT_EQ(data.sideDefs.size(), 4u);
	ASSERT_EQ(data.lineDefs.size(), 4u);
	ASSERT_EQ(data.things.size(), 1u);

	EXPECT_DOUBLE_EQ(data.vertices[2].x, 256);
	EXPECT_DOUBLE_EQ(data.vertices[2].y, 256);

	EXPECT_DOUBLE_EQ(data.sectors[0].floorHeight, 8);
	EXPECT_DOUBLE_EQ(data.sectors[0].ceilingHeight, 136);
	EXPECT_EQ(data.sectors[0].floorTexture, "FLOOR1");

	EXPECT_EQ(data.sideDefs[0].middleTexture, "WALL1");
	EXPECT_EQ(data.sideDefs[0].sector, 0);

	for (const auto& line : data.lineDefs)
	{
		EXPECT_EQ(line.sideBack, NO_INDEX);
	}

	EXPECT_EQ(data.things[0].type, 2035);
	EXPECT_DOUBLE_EQ(data.things[0].angle, 90);
}

TEST_F(DoomWadTest, ReadsUdmfLumps)
{
	std::string textMap = R"(
namespace = "Doom";

vertex { x = 0.0; y = 0.0; }
vertex { x = 0.0; y = 256.0; }
vertex { x = 256.0; y = 256.0; }
vertex { x = 256.0; y = 0.0; }

sector
{
	heightfloor = 8;
	heightceiling = 136;
	texturefloor = "FLOOR1";
	textureceiling = "CEIL1";
	lightlevel = 192;
}

sidedef { sector = 0; texturemiddle = "WALL1"; offsetx = 16; offsety = -8; }
sidedef { sector = 0; texturemiddle = "WALL1"; }
sidedef { sector = 0; texturemiddle = "WALL1"; }
sidedef { sector = 0; texturemiddle = "WALL1"; }

linedef { v1 = 0; v2 = 1; sidefront = 0; sideback = -1; dontpegbottom = true; }
linedef { v1 = 1; v2 = 2; sidefront = 1; sideback = -1; }
linedef { v1 = 2; v2 = 3; sidefront = 2; sideback = -1; }
linedef { v1 = 3; v2 = 0; sidefront = 3; sideback = -1; }

thing { x = 64.0; y = 96.0; angle = 90; type = 2035; }
)";

	auto contents = buildWad({ { "MAP01", "" }, { "TEXTMAP", textMap }, { "ENDMAP", "" } });

	std::istringstream stream(contents);

	WadFile wad(stream);

	ASSERT_EQ(wad.getMaps().size(), 1u);
	EXPECT_EQ(wad.getMaps().front().kind, MapLumpKind::Udmf);

	auto data = UdmfLoader::load(wad.readMapLump(wad.getMaps().front(), "TEXTMAP"));

	ASSERT_EQ(data.vertices.size(), 4u);
	ASSERT_EQ(data.sectors.size(), 1u);
	ASSERT_EQ(data.sideDefs.size(), 4u);
	ASSERT_EQ(data.lineDefs.size(), 4u);
	ASSERT_EQ(data.things.size(), 1u);

	EXPECT_DOUBLE_EQ(data.sectors[0].ceilingHeight, 136);
	EXPECT_EQ(data.sectors[0].lightLevel, 192);

	EXPECT_DOUBLE_EQ(data.sideDefs[0].offsetX, 16);
	EXPECT_DOUBLE_EQ(data.sideDefs[0].offsetY, -8);
	EXPECT_EQ(data.sideDefs[1].offsetX, 0);

	EXPECT_TRUE(data.lineDefs[0].dontPegBottom);
	EXPECT_FALSE(data.lineDefs[1].dontPegBottom);
	EXPECT_EQ(data.lineDefs[0].sideBack, NO_INDEX);

	EXPECT_EQ(data.things[0].type, 2035);
}

TEST_F(DoomWadTest, TracesSquareSector)
{
	auto data = loadBinary(buildSquareRoom(256).toWad());

	auto footprints = SectorGeometry::build(data, 8);

	ASSERT_EQ(footprints.size(), 1u);
	ASSERT_TRUE(footprints[0].valid);
	ASSERT_EQ(footprints[0].rings.size(), 1u);

	EXPECT_NEAR(polygon::ringArea(footprints[0].rings[0]), 256.0 * 256.0, 1.0);
	EXPECT_NEAR(totalPieceArea(footprints[0]), 256.0 * 256.0, 1.0);
}

TEST_F(DoomWadTest, TracesSectorPinchedAtASharedVertex)
{
	BinaryMapBuilder builder;

	builder.addVertex(0, 0);
	builder.addVertex(0, 256);
	builder.addVertex(256, 256);
	builder.addVertex(256, 0);
	builder.addVertex(256, 512);
	builder.addVertex(512, 512);
	builder.addVertex(512, 256);

	builder.addSector(0, 128);

	const int lines[8][2] = {
		{ 0, 1 }, { 1, 2 }, { 2, 3 }, { 3, 0 },
		{ 2, 4 }, { 4, 5 }, { 5, 6 }, { 6, 2 },
	};

	for (int i = 0; i < 8; ++i)
	{
		builder.addSideDef(0);
		builder.addLineDef(lines[i][0], lines[i][1], i, -1);
	}

	auto data = loadBinary(builder.toWad());

	auto footprints = SectorGeometry::build(data, 8);

	ASSERT_TRUE(footprints[0].valid);

	EXPECT_EQ(footprints[0].rings.size(), 2u);

	EXPECT_NEAR(totalRingArea(footprints[0]), 2 * 256.0 * 256.0, 1.0);
	EXPECT_NEAR(totalPieceArea(footprints[0]), 2 * 256.0 * 256.0, 1.0);

	for (const auto& ring : footprints[0].rings)
	{
		EXPECT_NEAR(polygon::ringArea(ring), 256.0 * 256.0, 1.0);
	}
}

TEST_F(DoomWadTest, AppliesThePrismMargin)
{
	auto data = loadBinary(buildSquareRoom(256, 0, 128).toWad());

	const double margin = 12;

	auto footprints = SectorGeometry::build(data, margin);

	ASSERT_TRUE(footprints[0].valid);

	EXPECT_DOUBLE_EQ(footprints[0].prismBottom, data.sectors[0].floorHeight - margin);
	EXPECT_DOUBLE_EQ(footprints[0].prismTop, data.sectors[0].ceilingHeight + margin);
}

TEST_F(DoomWadTest, TracesSectorWithHole)
{
	BinaryMapBuilder builder;

	builder.addVertex(0, 0);
	builder.addVertex(0, 256);
	builder.addVertex(256, 256);
	builder.addVertex(256, 0);

	builder.addVertex(64, 64);
	builder.addVertex(192, 64);
	builder.addVertex(192, 192);
	builder.addVertex(64, 192);

	builder.addSector(0, 128);

	for (int i = 0; i < 4; ++i)
	{
		builder.addSideDef(0);
		builder.addLineDef(i, (i + 1) % 4, i, -1);
	}

	for (int i = 0; i < 4; ++i)
	{
		builder.addSideDef(0);
		builder.addLineDef(4 + i, 4 + (i + 1) % 4, 4 + i, -1);
	}

	auto data = loadBinary(builder.toWad());

	auto footprints = SectorGeometry::build(data, 8);

	ASSERT_TRUE(footprints[0].valid);
	ASSERT_EQ(footprints[0].rings.size(), 2u);

	auto expected = 256.0 * 256.0 - 128.0 * 128.0;

	EXPECT_NEAR(totalRingArea(footprints[0]), expected, 1.0);
	EXPECT_NEAR(totalPieceArea(footprints[0]), expected, 1.0);
}

namespace
{

BinaryMapBuilder buildTwoRooms(int floorA, int ceilingA, int floorB, int ceilingB)
{
	BinaryMapBuilder builder;

	builder.addVertex(0, 0);
	builder.addVertex(0, 256);
	builder.addVertex(256, 256);
	builder.addVertex(256, 0);
	builder.addVertex(512, 256);
	builder.addVertex(512, 0);

	builder.addSector(floorA, ceilingA);
	builder.addSector(floorB, ceilingB);

	builder.addSideDef(0);
	builder.addLineDef(0, 1, 0, -1);
	builder.addSideDef(0);
	builder.addLineDef(1, 2, 1, -1);
	builder.addSideDef(0);
	builder.addLineDef(3, 0, 2, -1);

	builder.addSideDef(0);
	builder.addSideDef(1);
	builder.addLineDef(2, 3, 3, 4);

	builder.addSideDef(1);
	builder.addLineDef(2, 4, 5, -1);
	builder.addSideDef(1);
	builder.addLineDef(4, 5, 6, -1);
	builder.addSideDef(1);
	builder.addLineDef(5, 3, 7, -1);

	return builder;
}

}

TEST_F(DoomWadTest, PrismsReachPastEveryNeighbour)
{
	auto data = loadBinary(buildTwoRooms(0, 128, 64, 320).toWad());

	auto footprints = SectorGeometry::build(data, 8);

	ASSERT_EQ(footprints.size(), 2u);
	ASSERT_TRUE(footprints[0].valid);
	ASSERT_TRUE(footprints[1].valid);

	EXPECT_NEAR(totalPieceArea(footprints[0]), 256.0 * 256.0, 1.0);
	EXPECT_NEAR(totalPieceArea(footprints[1]), 256.0 * 256.0, 1.0);

	EXPECT_LE(footprints[0].prismBottom, data.sectors[1].floorHeight);
	EXPECT_GE(footprints[0].prismTop, data.sectors[1].ceilingHeight);
	EXPECT_LE(footprints[1].prismBottom, data.sectors[0].floorHeight);
	EXPECT_GE(footprints[1].prismTop, data.sectors[0].ceilingHeight);
}

TEST_F(DoomWadTest, PrismsStayTightAroundTheArchitecture)
{
	auto data = loadBinary(buildTwoRooms(0, 128, 64, 320).toWad());

	auto footprints = SectorGeometry::build(data, 8);

	double lowestFloor = std::min(data.sectors[0].floorHeight, data.sectors[1].floorHeight);
	double highestCeiling = std::max(data.sectors[0].ceilingHeight, data.sectors[1].ceilingHeight);

	for (const auto& footprint : footprints)
	{
		ASSERT_TRUE(footprint.valid);

		EXPECT_DOUBLE_EQ(footprint.prismBottom, lowestFloor - 8);
		EXPECT_DOUBLE_EQ(footprint.prismTop, highestCeiling + 8);
	}
}

TEST_F(DoomWadTest, ClosedNeighbourDoesNotStretchThePrism)
{
	auto data = loadBinary(buildTwoRooms(0, 128, -3000, -3000).toWad());

	auto footprints = SectorGeometry::build(data, 8);

	ASSERT_TRUE(footprints[0].valid);

	EXPECT_DOUBLE_EQ(footprints[0].prismBottom, data.sectors[0].floorHeight - 8);
	EXPECT_DOUBLE_EQ(footprints[0].prismTop, data.sectors[0].ceilingHeight + 8);
}

TEST_F(DoomWadTest, SkipsControlSectorsParkedOutsideTheMap)
{
	auto data = loadBinary(buildTwoRooms(0, 128, -16000, 16000).toWad());

	auto footprints = SectorGeometry::build(data, 8);

	ASSERT_EQ(footprints.size(), 2u);

	EXPECT_TRUE(footprints[0].valid);
	EXPECT_FALSE(footprints[1].valid);

	EXPECT_DOUBLE_EQ(footprints[0].prismBottom, data.sectors[0].floorHeight - 8);
	EXPECT_DOUBLE_EQ(footprints[0].prismTop, data.sectors[0].ceilingHeight + 8);
}

TEST_F(DoomWadTest, KeepsOrdinaryTallRoomsOutOfTheControlSectorRule)
{
	auto data = loadBinary(buildTwoRooms(0, 128, 0, 1024).toWad());

	auto footprints = SectorGeometry::build(data, 8);

	EXPECT_TRUE(footprints[0].valid);
	EXPECT_TRUE(footprints[1].valid);

	EXPECT_GE(footprints[0].prismTop, data.sectors[1].ceilingHeight);
}

TEST_F(DoomWadTest, ShellEnclosesTheFootprint)
{
	auto data = loadBinary(buildSquareRoom(256).toWad());

	auto footprints = SectorGeometry::build(data, 8);

	const double thickness = 16;

	auto shell = SectorGeometry::buildShell(footprints, thickness);

	ASSERT_FALSE(shell.empty());

	double shellArea = 0;

	for (const auto& piece : shell)
	{
		shellArea += std::fabs(polygon::ringArea(piece));
	}

	auto expected = (256.0 + 2 * thickness) * (256.0 + 2 * thickness) - 256.0 * 256.0;

	EXPECT_NEAR(shellArea, expected, expected * 0.02);
}

TEST_F(DoomWadTest, SpreadsLightsAcrossLargeRooms)
{
	auto data = loadBinary(buildSquareRoom(1024).toWad());

	auto footprints = SectorGeometry::build(data, 8);

	auto sparse = DoomLightPlacer::place(data, footprints, 512);
	auto dense = DoomLightPlacer::place(data, footprints, 128);

	EXPECT_GE(sparse.size(), 1u);
	EXPECT_GT(dense.size(), sparse.size());

	for (const auto& light : dense)
	{
		EXPECT_GE(light.position.x(), 0);
		EXPECT_LE(light.position.x(), 1024);
		EXPECT_GE(light.position.y(), 0);
		EXPECT_LE(light.position.y(), 1024);
		EXPECT_GT(light.height, data.sectors[0].floorHeight);
		EXPECT_LT(light.height, data.sectors[0].ceilingHeight);
	}
}

TEST_F(DoomWadTest, SuppressesLightsInCrowdedNeighbouringSectors)
{
	auto data = loadBinary(buildRoomRow(6, 64, 96, { 160 }).toWad());

	auto footprints = SectorGeometry::build(data, 8);

	ASSERT_EQ(footprints.size(), 6u);

	auto lights = DoomLightPlacer::place(data, footprints, 512);

	EXPECT_GE(lights.size(), 1u);
	EXPECT_LT(lights.size(), footprints.size());
	EXPECT_LE(lights.size(), 2u);
}

TEST_F(DoomWadTest, KeepsLightsWhereBrightnessDiffers)
{
	auto data = loadBinary(buildRoomRow(6, 64, 96, { 32, 255 }).toWad());

	auto footprints = SectorGeometry::build(data, 8);

	auto lights = DoomLightPlacer::place(data, footprints, 512);

	EXPECT_GE(lights.size(), 2u);

	bool sawDark = false;
	bool sawBright = false;

	for (const auto& light : lights)
	{
		if (light.lightLevel == 32) sawDark = true;
		if (light.lightLevel == 255) sawBright = true;
	}

	EXPECT_TRUE(sawDark);
	EXPECT_TRUE(sawBright);
}

TEST_F(DoomWadTest, RejectsTruncatedAndMalformedWads)
{
	auto valid = buildSquareRoom().toWad();

	auto truncated = valid.substr(0, valid.size() - 24);

	std::istringstream truncatedStream(truncated);
	EXPECT_THROW(WadFile wad(truncatedStream), map::IMapReader::FailureException);

	std::istringstream headerOnly(valid.substr(0, 8));
	EXPECT_THROW(WadFile wad(headerOnly), map::IMapReader::FailureException);

	std::istringstream notAWad("some other kind of file entirely");
	EXPECT_THROW(WadFile wad(notAWad), map::IMapReader::FailureException);
}

TEST_F(DoomWadTest, KeepsLightsApartAtTheGivenSpacing)
{
	auto data = loadBinary(buildSquareRoom(1024).toWad());

	auto footprints = SectorGeometry::build(data, 8);

	const double spacing = 192;

	auto lights = DoomLightPlacer::place(data, footprints, spacing);

	ASSERT_GT(lights.size(), 1u);

	for (std::size_t i = 0; i < lights.size(); ++i)
	{
		for (std::size_t j = i + 1; j < lights.size(); ++j)
		{
			auto distance = (lights[i].position - lights[j].position).getLength();

			EXPECT_GE(distance, spacing - 0.01);
		}
	}
}

using DoomBrushBuilderTest = RadiantTest;

TEST_F(DoomBrushBuilderTest, BuildsPrismsForEverySector)
{
	auto data = loadBinary(buildSquareRoom(256, 0, 128).toWad());

	auto footprints = SectorGeometry::build(data, 8);

	ASSERT_TRUE(footprints[0].valid);

	DoomBrushBuilder builder(data, footprints, 1.0);

	std::vector<scene::INodePtr> brushes;

	builder.buildSectorBrushes([&](const scene::INodePtr& brush) { brushes.push_back(brush); });

	ASSERT_EQ(brushes.size(), footprints[0].pieces.size() * 2);

	for (const auto& node : brushes)
	{
		auto* brush = Node_getIBrush(node);

		ASSERT_TRUE(brush != nullptr);

		EXPECT_GE(brush->getNumFaces(), 5u);
		EXPECT_TRUE(brush->hasContributingFaces());
	}
}

TEST_F(DoomBrushBuilderTest, BuildsSealingShellBrushes)
{
	auto data = loadBinary(buildSquareRoom(256).toWad());

	auto footprints = SectorGeometry::build(data, 8);

	auto shell = SectorGeometry::buildShell(footprints, 16);

	ASSERT_FALSE(shell.empty());

	DoomBrushBuilder builder(data, footprints, 1.0);

	std::vector<scene::INodePtr> brushes;

	builder.buildShellBrushes(shell, 16, [&](const scene::INodePtr& brush) { brushes.push_back(brush); });

	EXPECT_EQ(brushes.size(), shell.size());

	for (const auto& node : brushes)
	{
		EXPECT_TRUE(Node_getIBrush(node)->hasContributingFaces());
	}
}

TEST_F(DoomBrushBuilderTest, ScalesGeometryIntoWorldUnits)
{
	auto data = loadBinary(buildSquareRoom(256, 0, 128).toWad());

	auto footprints = SectorGeometry::build(data, 8);

	const double scale = 2.0;

	DoomBrushBuilder builder(data, footprints, scale);

	AABB bounds;

	builder.buildSectorBrushes([&](const scene::INodePtr& brush)
	{
		bounds.includeAABB(brush->localAABB());
	});

	EXPECT_NEAR(bounds.getExtents().x() * 2, 256.0 * scale, 1.0);
	EXPECT_NEAR(bounds.getExtents().y() * 2, 256.0 * scale, 1.0);
}

}
