#include "RadiantTest.h"

#include "map/format/goldsrc/GoldSrcLoader.h"
#include "map/format/ConversionMap.h"

#include "ibrush.h"
#include "icommandsystem.h"
#include "ilayer.h"
#include "imap.h"
#include "imapformat.h"
#include "iselectiongroup.h"
#include "messages/MapConversionRequest.h"
#include "algorithm/Primitives.h"
#include "algorithm/Scene.h"
#include "testutil/FileSelectionHelper.h"
#include "testutil/TemporaryFile.h"

#include <array>
#include <cmath>
#include <fstream>
#include <sstream>

namespace test
{

using namespace map::goldsrc;

namespace
{

struct BoxFace
{
	Vector3 a;
	Vector3 b;
	Vector3 c;
	Vector3 normal;
	Vector3 uAxis;
	Vector3 vAxis;
};

std::array<BoxFace, 6> boxFaces(const Vector3& min, const Vector3& max)
{
	double x0 = min.x(), y0 = min.y(), z0 = min.z();
	double x1 = max.x(), y1 = max.y(), z1 = max.z();

	return { {
		{ { x0, y0, z0 }, { x1, y0, z0 }, { x0, y1, z0 }, { 0, 0, -1 }, { 1, 0, 0 }, { 0, -1, 0 } },
		{ { x0, y0, z1 }, { x0, y1, z1 }, { x1, y0, z1 }, { 0, 0, 1 }, { 1, 0, 0 }, { 0, -1, 0 } },
		{ { x0, y0, z0 }, { x0, y1, z0 }, { x0, y0, z1 }, { -1, 0, 0 }, { 0, 1, 0 }, { 0, 0, -1 } },
		{ { x1, y0, z0 }, { x1, y0, z1 }, { x1, y1, z0 }, { 1, 0, 0 }, { 0, 1, 0 }, { 0, 0, -1 } },
		{ { x0, y0, z0 }, { x0, y0, z1 }, { x1, y0, z0 }, { 0, -1, 0 }, { 1, 0, 0 }, { 0, 0, -1 } },
		{ { x0, y1, z0 }, { x1, y1, z0 }, { x0, y1, z1 }, { 0, 1, 0 }, { 1, 0, 0 }, { 0, 0, -1 } },
	} };
}

class ByteWriter
{
public:
	std::string data;

	void i32(int32_t value)
	{
		append(&value, sizeof(value));
	}

	void f32(double value)
	{
		float raw = static_cast<float>(value);
		append(&raw, sizeof(raw));
	}

	void u8(int value)
	{
		data.push_back(static_cast<char>(value));
	}

	void zeros(std::size_t count)
	{
		data.append(count, '\0');
	}

	void vec(const Vector3& v)
	{
		f32(v.x());
		f32(v.y());
		f32(v.z());
	}

	void fixed(const std::string& text, std::size_t length)
	{
		data.append(text);
		zeros(length - text.size());
	}

	void byteString(const std::string& text)
	{
		u8(static_cast<int>(text.size() + 1));
		data.append(text);
		u8(0);
	}

	void intString(const std::string& text)
	{
		i32(static_cast<int32_t>(text.size()));
		data.append(text);
	}

private:
	void append(const void* source, std::size_t size)
	{
		data.append(static_cast<const char*>(source), size);
	}
};

class RmfWriter :
	public ByteWriter
{
	int _version;

public:
	double rotation = 15;

	RmfWriter(int version) :
		_version(version)
	{
		f32(version / 10.0);
		data.append("RMF");
	}

	void base(int visgroup, int childCount)
	{
		if (_version <= 9)
		{
			i32(visgroup > 0 ? 1 : 0);
			if (visgroup > 0) u8(visgroup);
		}
		else
		{
			i32(visgroup);
		}

		zeros(3);
		i32(childCount);
	}

	void solid(const Vector3& min, const Vector3& max, int visgroup, const std::string& texture = "c1a0_wall")
	{
		byteString("CMapSolid");
		base(visgroup, 0);
		i32(6);

		for (const auto& face : boxFaces(min, max))
		{
			fixed(texture, _version <= 16 ? 40 : 260);

			if (_version <= 18)
			{
				f32(rotation);
				f32(8);
				f32(4);
			}
			else
			{
				vec(face.uAxis);
				f32(8);
				vec(face.vAxis);
				f32(4);
				f32(0);
			}

			f32(1);
			f32(1);

			if (_version > 16) zeros(16);
			else if (_version > 9) zeros(4);

			i32(3);
			vec(face.a);
			vec(face.b);
			vec(face.c);
			vec(face.a);
			vec(face.b);
			vec(face.c);
		}
	}

	void entityData(const std::string& classname, const std::vector<std::pair<std::string, std::string>>& keyValues)
	{
		byteString(classname);
		zeros(4);
		i32(0);
		i32(static_cast<int32_t>(keyValues.size()));

		for (const auto& pair : keyValues)
		{
			byteString(pair.first);
			byteString(pair.second);
		}

		if (_version >= 16) zeros(12);
	}

	void entityTrailer(const Vector3& origin)
	{
		zeros(2);
		vec(origin);
		zeros(4);
	}

	void visgroup(int id, const std::string& name, bool visible = true)
	{
		fixed(name, 128);
		zeros(4);
		i32(id);
		u8(visible ? 1 : 0);
		zeros(3);
	}
};

class JmfWriter :
	public ByteWriter
{
public:
	JmfWriter()
	{
		data.append("JHMF");
		i32(121);
		i32(0);
	}

	void solid(const Vector3& min, const Vector3& max, int groupId, int visgroup, int curveCount)
	{
		i32(curveCount);
		i32(0);
		i32(groupId);
		i32(0);
		zeros(4);
		i32(1);
		i32(visgroup);
		i32(6);

		for (const auto& face : boxFaces(min, max))
		{
			i32(0);
			i32(3);
			vec(face.uAxis);
			f32(8);
			vec(face.vAxis);
			f32(4);
			f32(1);
			f32(1);
			f32(0);
			zeros(16);
			i32(0);
			fixed("c1a0_wall", 64);
			vec(face.normal);
			f32(face.normal.dot(face.a));
			i32(3);

			for (const auto& vertex : { face.a, face.b, face.c })
			{
				vec(vertex);
				zeros(12);
			}
		}

		zeros(curveCount * (8 + 128 + 4 + 32 * 32 * 36));
	}

	void entityHeader(const std::string& classname, const Vector3& origin, int groupId)
	{
		intString(classname);
		vec(origin);
		i32(0);
		i32(groupId);
		i32(0);
		zeros(4);

		for (int i = 0; i < 13; ++i)
		{
			intString("");
		}

		i32(0);
		zeros(76);
	}
};

std::string buildRmf22()
{
	RmfWriter rmf(22);

	rmf.i32(2);
	rmf.visgroup(1, "Walls");
	rmf.visgroup(2, "Doors");

	rmf.byteString("CMapWorld");
	rmf.base(0, 4);

	rmf.solid({ 0, 0, 0 }, { 64, 64, 64 }, 1);

	rmf.byteString("CMapGroup");
	rmf.base(0, 1);
	rmf.byteString("CMapGroup");
	rmf.base(0, 1);
	rmf.solid({ 128, 0, 0 }, { 192, 64, 64 }, 0);

	rmf.byteString("CMapEntity");
	rmf.base(2, 1);
	rmf.solid({ 256, 0, 0 }, { 320, 64, 64 }, 0);
	rmf.entityData("func_static", { { "name", "door" } });
	rmf.entityTrailer({ 0, 0, 0 });

	rmf.byteString("CMapEntity");
	rmf.base(0, 0);
	rmf.entityData("info_player_start", { { "name", "start" } });
	rmf.entityTrailer({ 128, 64, 32 });

	rmf.entityData("worldspawn", { { "wad", "halflife.wad" } });

	rmf.i32(1);
	rmf.fixed("path", 128);
	rmf.fixed("path_corner", 128);
	rmf.i32(0);
	rmf.i32(1);
	rmf.vec({ 0, 0, 0 });
	rmf.i32(1);
	rmf.fixed("path01", 128);
	rmf.i32(0);

	return rmf.data;
}

std::string buildRmfConversions()
{
	RmfWriter rmf(22);

	rmf.i32(2);
	rmf.visgroup(1, "Walls");
	rmf.visgroup(2, "Hidden", false);

	rmf.byteString("CMapWorld");
	rmf.base(0, 10);

	rmf.solid({ 0, 0, 0 }, { 64, 64, 64 }, 1);
	rmf.solid({ 512, 0, 0 }, { 576, 64, 64 }, 0, "HINT");
	rmf.solid({ 640, 0, 0 }, { 704, 64, 64 }, 0, "skip");
	rmf.solid({ 768, 0, 0 }, { 832, 64, 64 }, 0, "ORIGIN");

	rmf.byteString("CMapEntity");
	rmf.base(0, 2);
	rmf.solid({ 256, 0, 0 }, { 320, 64, 64 }, 0);
	rmf.solid({ 282, 29, 24 }, { 298, 45, 40 }, 0, "ORIGIN");
	rmf.entityData("func_door_rotating", { { "targetname", "door" }, { "angles", "-90 0 0" }, { "lip", "8" } });
	rmf.entityTrailer({ 0, 0, 0 });

	for (int i = 0; i < 2; ++i)
	{
		rmf.byteString("CMapEntity");
		rmf.base(2, 0);
		rmf.entityData("light", { { "targetname", "lamp" }, { "_light", "255 128 0 100" } });
		rmf.entityTrailer({ 32.0 + i * 64, 32, 32 });
	}

	rmf.byteString("CMapEntity");
	rmf.base(0, 0);
	rmf.entityData("trigger_relay", { { "targetname", "relay" }, { "target", "lamp" } });
	rmf.entityTrailer({ 0, 0, 0 });

	rmf.byteString("CMapEntity");
	rmf.base(0, 0);
	rmf.entityData("light_spot", { { "targetname", "spot" }, { "_light", "255 255 255 200" },
		{ "angles", "-90 0 0" }, { "_cone2", "45" } });
	rmf.entityTrailer({ 0, 0, 128 });

	rmf.byteString("CMapEntity");
	rmf.base(0, 0);
	rmf.entityData("info_player_start", { { "targetname", "start" }, { "angles", "0 90 0" } });
	rmf.entityTrailer({ 0, 0, 0 });

	rmf.entityData("worldspawn", {});
	rmf.i32(0);

	return rmf.data;
}

std::string buildRmfSolidity()
{
	RmfWriter rmf(22);

	rmf.i32(0);

	const std::vector<std::pair<std::string, std::vector<std::pair<std::string, std::string>>>> entities = {
		{ "func_illusionary", { { "name", "rail" } } },
		{ "func_detail", { { "name", "detail" }, { "zhlt_noclip", "1" } } },
		{ "info_location", { { "name", "location" } } },
		{ "func_wall", { { "name", "wall" } } },
		{ "func_ladder", { { "name", "ladder" } } },
		{ "func_monsterclip", { { "name", "monsterclip" } } },
	};

	rmf.byteString("CMapWorld");
	rmf.base(0, static_cast<int>(entities.size()));

	double x = 0;

	for (const auto& [className, keyValues] : entities)
	{
		rmf.byteString("CMapEntity");
		rmf.base(0, 1);
		rmf.solid({ x, 0, 0 }, { x + 64, 64, 64 }, 0, "AAATRIGGER");
		rmf.entityData(className, keyValues);
		rmf.entityTrailer({ 0, 0, 0 });
		x += 128;
	}

	rmf.entityData("worldspawn", {});
	rmf.i32(0);

	return rmf.data;
}

std::string buildWad(const std::string& texture, int width, int height)
{
	ByteWriter wad;
	wad.data.append("WAD3");
	wad.i32(1);
	wad.i32(12 + 40);

	wad.fixed(texture, 16);
	wad.i32(width);
	wad.i32(height);
	wad.zeros(16);

	wad.i32(12);
	wad.i32(40);
	wad.i32(40);
	wad.u8(0x43);
	wad.u8(0);
	wad.zeros(2);
	wad.fixed(texture, 16);

	return wad.data;
}

void writeBinary(const std::string& path, const std::string& contents)
{
	std::ofstream stream(path, std::ios::out | std::ios::binary);
	stream << contents;
}

std::vector<double> numbers(const std::string& value)
{
	std::vector<double> result;
	std::stringstream stream(value);
	double number;

	while (stream >> number) result.push_back(number);

	return result;
}

bool faceHasVertexWithFractionalUV(const IFace* face, const Vector3& xyz, const Vector2& uv)
{
	return algorithm::faceHasVertex(face, [&](const WindingVertex& vertex)
	{
		auto fraction = [](double value) { return value - std::floor(value); };

		return math::isNear(vertex.vertex, xyz, 0.01) &&
			std::abs(fraction(vertex.texcoord.x()) - fraction(uv.x())) < 0.01 &&
			std::abs(fraction(vertex.texcoord.y()) - fraction(uv.y())) < 0.01;
	});
}

std::string buildJmf()
{
	JmfWriter jmf;

	jmf.i32(2);
	for (auto [id, parent] : { std::pair<int, int>{ 5, 0 }, std::pair<int, int>{ 6, 5 } })
	{
		jmf.i32(id);
		jmf.i32(parent);
		jmf.i32(0);
		jmf.i32(1);
		jmf.zeros(4);
	}

	jmf.i32(1);
	jmf.intString("Detail");
	jmf.i32(7);
	jmf.zeros(4);
	jmf.u8(1);

	jmf.zeros(24);
	jmf.i32(0);

	jmf.i32(1);
	jmf.intString("path_corner");
	jmf.intString("path");
	jmf.i32(0);
	jmf.i32(0);
	jmf.zeros(4);
	jmf.i32(1);
	jmf.intString("path01");
	jmf.intString("");
	jmf.zeros(12 + 12 + 4 + 4);
	jmf.i32(0);

	jmf.entityHeader("info_player_start", { 16, 32, 48 }, 0);
	jmf.i32(0);
	jmf.i32(0);
	jmf.i32(0);

	jmf.entityHeader("worldspawn", { 0, 0, 0 }, 0);
	jmf.i32(1);
	jmf.intString("wad");
	jmf.intString("halflife.wad");
	jmf.i32(0);
	jmf.i32(1);
	jmf.solid({ 0, 0, 0 }, { 64, 64, 64 }, 6, 7, 1);

	return jmf.data;
}

std::string keyValue(const map::goldsrc::Entity& entity, const std::string& key)
{
	for (const auto& pair : entity.keyValues)
	{
		if (pair.first == key) return pair.second;
	}

	return {};
}

const map::goldsrc::Face* faceWithNormal(const Solid& solid, const Vector3& normal)
{
	for (const auto& face : solid.faces)
	{
		if (math::isNear(face.plane.normal(), normal, 1e-6)) return &face;
	}

	return nullptr;
}

}

using GoldSrcLoaderTest = RadiantTest;

TEST_F(GoldSrcLoaderTest, ReadsRmfWorldEntitiesAndGroups)
{
	auto data = load(buildRmf22());

	EXPECT_EQ(keyValue(data.world, "classname"), "worldspawn");
	EXPECT_EQ(keyValue(data.world, "wad"), "halflife.wad");

	ASSERT_EQ(data.world.solids.size(), 2u);
	EXPECT_EQ(data.world.solids[0].visgroups, std::vector<int>{ 1 });
	EXPECT_TRUE(data.world.solids[0].groups.empty());

	ASSERT_EQ(data.world.solids[1].groups.size(), 2u);
	EXPECT_NE(data.world.solids[1].groups[0], data.world.solids[1].groups[1]);

	ASSERT_EQ(data.entities.size(), 2u);
	EXPECT_EQ(keyValue(data.entities[0], "classname"), "func_static");
	EXPECT_EQ(data.entities[0].visgroups, std::vector<int>{ 2 });
	EXPECT_EQ(data.entities[0].solids.size(), 1u);
	EXPECT_EQ(keyValue(data.entities[0], "origin"), "");
	EXPECT_EQ(keyValue(data.entities[1], "origin"), "128 64 32");

	ASSERT_EQ(data.visgroups.size(), 2u);
	EXPECT_EQ(data.visgroups[1].id, 2);
	EXPECT_EQ(data.visgroups[1].name, "Doors");

	EXPECT_EQ(data.skippedPaths, 1u);
}

TEST_F(GoldSrcLoaderTest, ReadsRmfPlanesFacingOutwards)
{
	auto data = load(buildRmf22());

	const auto& solid = data.world.solids[0];
	ASSERT_EQ(solid.faces.size(), 6u);

	auto top = faceWithNormal(solid, { 0, 0, 1 });
	ASSERT_TRUE(top != nullptr);
	EXPECT_NEAR(top->plane.dist(), 64, 1e-6);
	EXPECT_TRUE(top->valveAxes);
	EXPECT_EQ(top->texture, "c1a0_wall");
	EXPECT_NEAR(top->shiftU, 8, 1e-6);

	auto bottom = faceWithNormal(solid, { 0, 0, -1 });
	ASSERT_TRUE(bottom != nullptr);
	EXPECT_NEAR(bottom->plane.dist(), 0, 1e-6);
}

TEST_F(GoldSrcLoaderTest, ReadsQuakeStyleRmfFaces)
{
	RmfWriter rmf(18);
	rmf.i32(0);
	rmf.byteString("CMapWorld");
	rmf.base(0, 1);
	rmf.solid({ 0, 0, 0 }, { 64, 64, 64 }, 0);
	rmf.entityData("worldspawn", {});
	rmf.i32(0);

	auto data = load(rmf.data);

	ASSERT_EQ(data.world.solids.size(), 1u);
	ASSERT_EQ(data.world.solids[0].faces.size(), 6u);

	const auto& face = data.world.solids[0].faces[0];
	EXPECT_FALSE(face.valveAxes);
	EXPECT_NEAR(face.rotation, 15, 1e-6);
	EXPECT_NEAR(face.shiftU, 8, 1e-6);
	EXPECT_NEAR(face.shiftV, 4, 1e-6);
	EXPECT_EQ(face.texture, "c1a0_wall");
}

TEST_F(GoldSrcLoaderTest, ReadsVisgroupsStoredInTheOldRmfWorld)
{
	RmfWriter rmf(9);
	rmf.byteString("CMapWorld");
	rmf.base(0, 1);
	rmf.solid({ 0, 0, 0 }, { 64, 64, 64 }, 3);
	rmf.entityData("worldspawn", {});

	rmf.i32(1);
	rmf.fixed("Legacy", 128);
	rmf.zeros(4);
	rmf.u8(3);
	rmf.u8(1);
	rmf.zeros(2);

	auto data = load(rmf.data);

	ASSERT_EQ(data.world.solids.size(), 1u);
	EXPECT_EQ(data.world.solids[0].visgroups, std::vector<int>{ 3 });
	EXPECT_EQ(data.world.solids[0].faces.size(), 6u);

	ASSERT_EQ(data.visgroups.size(), 1u);
	EXPECT_EQ(data.visgroups[0].id, 3);
	EXPECT_EQ(data.visgroups[0].name, "Legacy");
}

TEST_F(GoldSrcLoaderTest, ReadsJmfWorldGroupsAndVisgroups)
{
	auto data = load(buildJmf());

	EXPECT_EQ(keyValue(data.world, "classname"), "worldspawn");
	EXPECT_EQ(keyValue(data.world, "wad"), "halflife.wad");

	ASSERT_EQ(data.world.solids.size(), 1u);
	EXPECT_EQ(data.world.solids[0].groups, (std::vector<int>{ 5, 6 }));
	EXPECT_EQ(data.world.solids[0].visgroups, std::vector<int>{ 7 });

	auto top = faceWithNormal(data.world.solids[0], { 0, 0, 1 });
	ASSERT_TRUE(top != nullptr);
	EXPECT_NEAR(top->plane.dist(), 64, 1e-6);
	EXPECT_TRUE(top->valveAxes);

	ASSERT_EQ(data.entities.size(), 1u);
	EXPECT_EQ(keyValue(data.entities[0], "origin"), "16 32 48");

	ASSERT_EQ(data.visgroups.size(), 1u);
	EXPECT_EQ(data.visgroups[0].name, "Detail");

	EXPECT_EQ(data.skippedPaths, 1u);
	EXPECT_EQ(data.skippedCurves, 1u);
}

TEST_F(GoldSrcLoaderTest, RejectsTruncatedAndMalformedFiles)
{
	auto rmf = buildRmf22();
	auto jmf = buildJmf();

	EXPECT_THROW(load(""), map::IMapReader::FailureException);
	EXPECT_THROW(load(rmf.substr(0, rmf.size() - 10)), map::IMapReader::FailureException);
	EXPECT_THROW(load(jmf.substr(0, jmf.size() - 10)), map::IMapReader::FailureException);

	auto badHeader = rmf;
	badHeader[4] = 'X';
	EXPECT_THROW(load(badHeader), map::IMapReader::FailureException);

	auto badVersion = jmf;
	badVersion[4] = 99;
	EXPECT_THROW(load(badVersion), map::IMapReader::FailureException);
}

TEST_F(GoldSrcLoaderTest, ReadsSolidBoundsAndVisgroupVisibility)
{
	auto data = load(buildRmfConversions());

	const auto& bounds = data.world.solids[0].bounds;
	EXPECT_TRUE(math::isNear(bounds.getOrigin(), Vector3(32, 32, 32), 1e-6));
	EXPECT_TRUE(math::isNear(bounds.getExtents(), Vector3(32, 32, 32), 1e-6));

	ASSERT_EQ(data.visgroups.size(), 2u);
	EXPECT_TRUE(data.visgroups[0].visible);
	EXPECT_FALSE(data.visgroups[1].visible);
}

TEST_F(GoldSrcLoaderTest, ReadsWadTextureSizes)
{
	fs::path wadPath = _context.getTemporaryDataPath();
	wadPath /= "GoldSrc_Test.WAD";
	TemporaryFile wadFile(wadPath.string());
	writeBinary(wadPath.string(), buildWad("C1A0_WALL", 32, 16));

	auto mapPath = (fs::path(_context.getTemporaryDataPath()) / "map.rmf").string();
	auto sizes = readWadTextureSizes(mapPath);

	ASSERT_EQ(sizes.count("C1A0_WALL"), 1u);
	EXPECT_EQ(sizes["C1A0_WALL"], std::make_pair(32.0, 16.0));
}

TEST_F(GoldSrcLoaderTest, ScalesDistanceKeys)
{
	map::ConversionMap::setScale(2);

	EXPECT_EQ(map::ConversionMap::scaleSpatialValue("lip", "8"), "16");
	EXPECT_EQ(map::ConversionMap::scaleSpatialValue("height", "-32"), "-64");
	EXPECT_EQ(map::ConversionMap::scaleSpatialValue("speed", "100"), "100");

	map::ConversionMap::clear();
}

class GoldSrcImportTest :
	public RadiantTest
{
protected:
	void openWithScale(const std::string& contents, const std::string& filename,
		const std::string& formatName, double scale)
	{
		fs::path mapPath = _context.getTemporaryDataPath();
		mapPath /= filename;
		TemporaryFile tempFile(mapPath.string());

		writeBinary(mapPath.string(), contents);

		auto format = GlobalMapFormatManager().getMapFormatByName(formatName);
		ASSERT_TRUE(format) << "Map format not registered: " << formatName;

		FileSelectionHelper responder(mapPath.string(), format);

		auto subscription = GlobalRadiantCore().getMessageBus().addListener(
			radiant::IMessage::Type::MapConversionRequest,
			radiant::TypeListener<radiant::MapConversionRequest>(
				[scale](radiant::MapConversionRequest& msg)
		{
			radiant::MapConversionRequest::Result result;
			result.accepted = true;
			result.scale = scale;

			msg.setResult(result);
			msg.setHandled(true);
		}));

		GlobalCommandSystem().executeCommand("OpenMap");

		GlobalRadiantCore().getMessageBus().removeListener(subscription);
	}
};

TEST_F(GoldSrcImportTest, MapsVisgroupsToLayers)
{
	openWithScale(buildRmf22(), "goldsrc_layers.rmf", "Worldcraft RMF", 1.0);

	auto root = GlobalMapModule().getRoot();
	auto& layers = root->getLayerManager();

	auto walls = layers.getLayerID("Walls");
	auto doors = layers.getLayerID("Doors");

	ASSERT_NE(walls, -1);
	ASSERT_NE(doors, -1);

	auto door = algorithm::getEntityByName(root, "door");
	ASSERT_TRUE(door);
	EXPECT_EQ(door->getLayers(), scene::LayerList{ doors });

	auto doorBrush = algorithm::findFirstBrush(door, [](const IBrushNodePtr&) { return true; });
	ASSERT_TRUE(doorBrush);
	EXPECT_EQ(doorBrush->getLayers(), scene::LayerList{ doors });

	auto wall = algorithm::findFirstBrush(root, [](const IBrushNodePtr& brush)
	{
		return std::dynamic_pointer_cast<scene::INode>(brush)->worldAABB().getOrigin().x() < 64;
	});
	ASSERT_TRUE(wall);
	EXPECT_EQ(wall->getLayers(), scene::LayerList{ walls });
}

TEST_F(GoldSrcImportTest, MapsGroupsToNestedSelectionGroups)
{
	openWithScale(buildRmf22(), "goldsrc_groups.rmf", "Worldcraft RMF", 1.0);

	auto root = GlobalMapModule().getRoot();

	auto grouped = algorithm::findFirstBrush(root, [](const IBrushNodePtr& brush)
	{
		auto x = std::dynamic_pointer_cast<scene::INode>(brush)->worldAABB().getOrigin().x();
		return x > 128 && x < 192;
	});
	ASSERT_TRUE(grouped);

	auto selectable = std::dynamic_pointer_cast<IGroupSelectable>(grouped);
	ASSERT_TRUE(selectable);
	EXPECT_EQ(selectable->getGroupIds().size(), 2u);

	auto door = std::dynamic_pointer_cast<IGroupSelectable>(algorithm::getEntityByName(root, "door"));
	ASSERT_TRUE(door);
	EXPECT_TRUE(door->getGroupIds().empty());
}

TEST_F(GoldSrcImportTest, ScalesRmfGeometryAndOrigins)
{
	openWithScale(buildRmf22(), "goldsrc_scaled.rmf", "Worldcraft RMF", 2.0);

	auto root = GlobalMapModule().getRoot();

	auto start = algorithm::getEntityByName(root, "start");
	ASSERT_TRUE(start);
	EXPECT_EQ(start->tryGetEntity()->getKeyValue("origin"), "256 128 64");

	auto wall = algorithm::findFirstBrush(root, [](const IBrushNodePtr& brush)
	{
		return std::dynamic_pointer_cast<scene::INode>(brush)->worldAABB().getOrigin().x() < 128;
	});
	ASSERT_TRUE(wall);

	auto bounds = wall->worldAABB();
	EXPECT_NEAR(bounds.getOrigin().x(), 64, 0.01);
	EXPECT_NEAR(bounds.getExtents().x(), 64, 0.01);
	EXPECT_NEAR(bounds.getExtents().z(), 64, 0.01);
}

TEST_F(GoldSrcImportTest, ImportsJmfWithScale)
{
	openWithScale(buildJmf(), "goldsrc_scaled.jmf", "Jackhammer JMF", 2.0);

	auto root = GlobalMapModule().getRoot();

	auto detail = root->getLayerManager().getLayerID("Detail");
	ASSERT_NE(detail, -1);

	auto brush = algorithm::findFirstBrush(root, [](const IBrushNodePtr&) { return true; });
	ASSERT_TRUE(brush);
	EXPECT_EQ(brush->getLayers(), scene::LayerList{ detail });
	EXPECT_EQ(std::dynamic_pointer_cast<IGroupSelectable>(brush)->getGroupIds().size(), 2u);

	auto bounds = brush->worldAABB();
	EXPECT_NEAR(bounds.getExtents().x(), 64, 0.01);
	EXPECT_NEAR(bounds.getOrigin().z(), 64, 0.01);
}

TEST_F(GoldSrcImportTest, ConvertsOriginBrushesToEntityOrigins)
{
	openWithScale(buildRmfConversions(), "goldsrc_origin.rmf", "Worldcraft RMF", 1.0);

	auto door = algorithm::getEntityByName(GlobalMapModule().getRoot(), "door");
	ASSERT_TRUE(door);

	auto& entity = *door->tryGetEntity();
	EXPECT_EQ(entity.getKeyValue("origin"), "290 37 32");
	EXPECT_EQ(entity.getKeyValue("movedir"), "-1");
	EXPECT_EQ(entity.getKeyValue("angles"), "");
	EXPECT_EQ(entity.getKeyValue("lip"), "8");

	std::size_t brushCount = 0;
	scene::INodePtr brush;

	door->foreachNode([&](const scene::INodePtr& node)
	{
		if (Node_isBrush(node))
		{
			brushCount++;
			brush = node;
		}
		return true;
	});

	ASSERT_EQ(brushCount, 1u);
	EXPECT_TRUE(math::isNear(brush->worldAABB().getOrigin(), Vector3(288, 32, 32), 0.01));
	EXPECT_TRUE(math::isNear(brush->worldAABB().getExtents(), Vector3(32, 32, 32), 0.01));
}

TEST_F(GoldSrcImportTest, ScalesOriginsAndDistanceKeysOfBrushEntities)
{
	openWithScale(buildRmfConversions(), "goldsrc_origin_scaled.rmf", "Worldcraft RMF", 2.0);

	auto door = algorithm::getEntityByName(GlobalMapModule().getRoot(), "door");
	ASSERT_TRUE(door);

	EXPECT_EQ(door->tryGetEntity()->getKeyValue("origin"), "580 74 64");
	EXPECT_EQ(door->tryGetEntity()->getKeyValue("lip"), "16");

	auto brush = algorithm::findFirstBrush(door, [](const IBrushNodePtr&) { return true; });
	ASSERT_TRUE(brush);
	EXPECT_TRUE(math::isNear(brush->worldAABB().getOrigin(), Vector3(576, 64, 64), 0.01));
}

TEST_F(GoldSrcImportTest, ConvertsLightsAnglesAndTargets)
{
	openWithScale(buildRmfConversions(), "goldsrc_keys.rmf", "Worldcraft RMF", 1.0);

	auto root = GlobalMapModule().getRoot();

	auto lamp1 = algorithm::getEntityByName(root, "lamp_1");
	auto lamp2 = algorithm::getEntityByName(root, "lamp_2");
	ASSERT_TRUE(lamp1);
	ASSERT_TRUE(lamp2);

	EXPECT_EQ(lamp1->tryGetEntity()->getKeyValue("_color"), "1 0.501961 0");
	EXPECT_EQ(lamp1->tryGetEntity()->getKeyValue("light_radius"), "200 200 200");
	EXPECT_EQ(lamp1->tryGetEntity()->getKeyValue("_light"), "");

	auto relay = algorithm::getEntityByName(root, "relay");
	ASSERT_TRUE(relay);
	EXPECT_EQ(relay->tryGetEntity()->getKeyValue("target"), "lamp_1");
	EXPECT_EQ(relay->tryGetEntity()->getKeyValue("target1"), "lamp_2");

	auto spot = algorithm::getEntityByName(root, "spot");
	ASSERT_TRUE(spot);

	auto target = numbers(spot->tryGetEntity()->getKeyValue("light_target"));
	auto right = numbers(spot->tryGetEntity()->getKeyValue("light_right"));
	ASSERT_EQ(target.size(), 3u);
	ASSERT_EQ(right.size(), 3u);
	EXPECT_NEAR(target[2], -400, 0.01);
	EXPECT_NEAR(Vector3(right[0], right[1], right[2]).getLength(), 400, 0.01);

	auto start = algorithm::getEntityByName(root, "start");
	ASSERT_TRUE(start);
	EXPECT_EQ(start->tryGetEntity()->getKeyValue("angle"), "90");
	EXPECT_EQ(start->tryGetEntity()->getKeyValue("angles"), "");
}

TEST_F(GoldSrcImportTest, DropsHintSkipAndOriginBrushesFromTheWorld)
{
	openWithScale(buildRmfConversions(), "goldsrc_hint.rmf", "Worldcraft RMF", 1.0);

	auto worldspawn = algorithm::findWorldspawn(GlobalMapModule().getRoot());
	ASSERT_TRUE(worldspawn);

	std::size_t brushCount = 0;

	worldspawn->foreachNode([&](const scene::INodePtr& node)
	{
		if (Node_isBrush(node)) brushCount++;
		return true;
	});

	EXPECT_EQ(brushCount, 1u);
}

TEST_F(GoldSrcImportTest, MakesNonSolidGoldSrcEntitiesNonSolid)
{
	openWithScale(buildRmfSolidity(), "goldsrc_solidity.rmf", "Worldcraft RMF", 1.0);

	auto root = GlobalMapModule().getRoot();

	for (const char* name : { "rail", "detail", "location" })
	{
		auto entity = algorithm::getEntityByName(root, name);
		ASSERT_TRUE(entity) << name;
		EXPECT_EQ(entity->tryGetEntity()->getKeyValue("solid"), "0") << name;
		EXPECT_EQ(entity->tryGetEntity()->getKeyValue("zhlt_noclip"), "") << name;
	}

	auto wall = algorithm::getEntityByName(root, "wall");
	ASSERT_TRUE(wall);
	EXPECT_EQ(wall->tryGetEntity()->getKeyValue("solid"), "");
}

TEST_F(GoldSrcImportTest, UsesDoom3ClipMaterialsForLaddersAndMonsterClip)
{
	openWithScale(buildRmfSolidity(), "goldsrc_clips.rmf", "Worldcraft RMF", 1.0);

	auto root = GlobalMapModule().getRoot();

	for (const auto& [name, material] : { std::pair<const char*, const char*>{ "ladder", "textures/common/ladder" },
		std::pair<const char*, const char*>{ "monsterclip", "textures/common/monster_clip" } })
	{
		auto entity = algorithm::getEntityByName(root, name);
		ASSERT_TRUE(entity) << name;

		auto brush = algorithm::findFirstBrush(entity, [](const IBrushNodePtr&) { return true; });
		ASSERT_TRUE(brush) << name;

		auto* ibrush = Node_getIBrush(brush);

		for (std::size_t i = 0; i < ibrush->getNumFaces(); ++i)
		{
			EXPECT_EQ(ibrush->getFace(i).getShader(), material) << name;
		}
	}
}

TEST_F(GoldSrcImportTest, AlignsQuakeStyleTexturesOfOldRmfVersions)
{
	fs::path wadPath = _context.getTemporaryDataPath();
	wadPath /= "GoldSrc_Test.WAD";
	TemporaryFile wadFile(wadPath.string());
	writeBinary(wadPath.string(), buildWad("C1A0_WALL", 32, 32));

	RmfWriter rmf(18);
	rmf.rotation = 0;
	rmf.i32(0);
	rmf.byteString("CMapWorld");
	rmf.base(0, 1);
	rmf.solid({ 0, 0, 0 }, { 64, 64, 64 }, 0);
	rmf.entityData("worldspawn", {});
	rmf.i32(0);

	openWithScale(rmf.data, "goldsrc_quake_style.rmf", "Worldcraft RMF", 1.0);

	auto brush = algorithm::findFirstBrush(GlobalMapModule().getRoot(), [](const IBrushNodePtr&) { return true; });
	ASSERT_TRUE(brush);

	auto top = algorithm::findBrushFaceWithNormal(Node_getIBrush(brush), Vector3(0, 0, 1));
	ASSERT_TRUE(top != nullptr);
	EXPECT_TRUE(algorithm::faceHasVertex(top, Vector3(0, 0, 64), Vector2(0.25, 0.125)));
	EXPECT_TRUE(algorithm::faceHasVertex(top, Vector3(64, 64, 64), Vector2(2.25, -1.875)));
}

TEST_F(GoldSrcImportTest, HidesLayersOfHiddenVisgroups)
{
	openWithScale(buildRmfConversions(), "goldsrc_hidden.rmf", "Worldcraft RMF", 1.0);

	auto& layers = GlobalMapModule().getRoot()->getLayerManager();

	auto walls = layers.getLayerID("Walls");
	auto hidden = layers.getLayerID("Hidden");
	ASSERT_NE(walls, -1);
	ASSERT_NE(hidden, -1);

	EXPECT_TRUE(layers.layerIsVisible(walls));
	EXPECT_FALSE(layers.layerIsVisible(hidden));
}

TEST_F(GoldSrcImportTest, AlignsTexturesToWadSizesAndEntityOrigins)
{
	fs::path wadPath = _context.getTemporaryDataPath();
	wadPath /= "GoldSrc_Test.WAD";
	TemporaryFile wadFile(wadPath.string());
	writeBinary(wadPath.string(), buildWad("C1A0_WALL", 32, 32));

	openWithScale(buildRmfConversions(), "goldsrc_alignment.rmf", "Worldcraft RMF", 1.0);

	auto root = GlobalMapModule().getRoot();

	auto wall = algorithm::findFirstBrush(root, [](const IBrushNodePtr& brush)
	{
		return std::dynamic_pointer_cast<scene::INode>(brush)->worldAABB().getOrigin().x() < 64;
	});
	ASSERT_TRUE(wall);

	auto wallTop = algorithm::findBrushFaceWithNormal(Node_getIBrush(wall), Vector3(0, 0, 1));
	ASSERT_TRUE(wallTop != nullptr);
	EXPECT_TRUE(algorithm::faceHasVertex(wallTop, Vector3(0, 0, 64), Vector2(0.25, 0.125)));
	EXPECT_TRUE(algorithm::faceHasVertex(wallTop, Vector3(64, 64, 64), Vector2(2.25, -1.875)));

	auto door = algorithm::getEntityByName(root, "door");
	ASSERT_TRUE(door);

	auto doorBrush = algorithm::findFirstBrush(door, [](const IBrushNodePtr&) { return true; });
	ASSERT_TRUE(doorBrush);

	auto doorTop = algorithm::findBrushFaceWithNormal(Node_getIBrush(doorBrush), Vector3(0, 0, 1));
	ASSERT_TRUE(doorTop != nullptr);
	EXPECT_TRUE(faceHasVertexWithFractionalUV(doorTop, Vector3(256, 0, 64), Vector2(0.25, 0.125)));
	EXPECT_TRUE(faceHasVertexWithFractionalUV(doorTop, Vector3(320, 64, 64), Vector2(0.25, 0.125)));
}

}
