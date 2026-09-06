#include "DoomBinaryLoader.h"

#include "imapformat.h"
#include "i18n.h"
#include "itextstream.h"
#include "string/case_conv.h"

#include <fmt/format.h>

namespace map
{

namespace doom
{

namespace
{

const std::size_t VERTEX_SIZE = 4;
const std::size_t SECTOR_SIZE = 26;
const std::size_t SIDEDEF_SIZE = 30;
const std::size_t LINEDEF_SIZE = 14;
const std::size_t LINEDEF_SIZE_HEXEN = 16;
const std::size_t THING_SIZE = 10;
const std::size_t THING_SIZE_HEXEN = 20;

const uint16_t NO_SIDE = 0xFFFF;

const uint16_t ML_DONTPEGTOP = 0x0008;
const uint16_t ML_DONTPEGBOTTOM = 0x0010;

uint16_t readUInt16(const std::vector<char>& data, std::size_t offset)
{
	return static_cast<uint16_t>(
		static_cast<unsigned char>(data[offset]) |
		(static_cast<unsigned char>(data[offset + 1]) << 8));
}

int16_t readInt16(const std::vector<char>& data, std::size_t offset)
{
	return static_cast<int16_t>(readUInt16(data, offset));
}

std::string readName(const std::vector<char>& data, std::size_t offset)
{
	std::size_t length = 0;

	while (length < 8 && data[offset + length] != '\0')
	{
		length++;
	}

	return string::to_upper_copy(std::string(&data[offset], length));
}

int toSideIndex(uint16_t raw)
{
	return raw == NO_SIDE ? NO_INDEX : static_cast<int>(raw);
}

std::size_t entryCount(const std::vector<char>& lump, std::size_t recordSize,
	const std::string& lumpName, const std::string& mapName)
{
	if (lump.empty())
	{
		throw IMapReader::FailureException(
			fmt::format(_("Map {0} is missing its {1} lump"), mapName, lumpName));
	}

	return lump.size() / recordSize;
}

}

DoomMapData DoomBinaryLoader::load(WadFile& wad, const WadMapGroup& group)
{
	bool isHexen = group.kind == MapLumpKind::HexenBinary;

	DoomMapData data;

	auto vertexes = wad.readMapLump(group, "VERTEXES");
	auto sectors = wad.readMapLump(group, "SECTORS");
	auto sideDefs = wad.readMapLump(group, "SIDEDEFS");
	auto lineDefs = wad.readMapLump(group, "LINEDEFS");
	auto things = wad.readMapLump(group, "THINGS");

	auto vertexCount = entryCount(vertexes, VERTEX_SIZE, "VERTEXES", group.name);
	auto sectorCount = entryCount(sectors, SECTOR_SIZE, "SECTORS", group.name);
	auto sideCount = entryCount(sideDefs, SIDEDEF_SIZE, "SIDEDEFS", group.name);

	auto lineSize = isHexen ? LINEDEF_SIZE_HEXEN : LINEDEF_SIZE;
	auto lineCount = entryCount(lineDefs, lineSize, "LINEDEFS", group.name);

	data.vertices.reserve(vertexCount);

	for (std::size_t i = 0; i < vertexCount; ++i)
	{
		auto offset = i * VERTEX_SIZE;

		DoomVertex vertex;
		vertex.x = readInt16(vertexes, offset);
		vertex.y = readInt16(vertexes, offset + 2);

		data.vertices.push_back(vertex);
	}

	data.sectors.reserve(sectorCount);

	for (std::size_t i = 0; i < sectorCount; ++i)
	{
		auto offset = i * SECTOR_SIZE;

		DoomSector sector;
		sector.floorHeight = readInt16(sectors, offset);
		sector.ceilingHeight = readInt16(sectors, offset + 2);
		sector.floorTexture = readName(sectors, offset + 4);
		sector.ceilingTexture = readName(sectors, offset + 12);
		sector.lightLevel = readInt16(sectors, offset + 20);

		data.sectors.push_back(sector);
	}

	data.sideDefs.reserve(sideCount);

	for (std::size_t i = 0; i < sideCount; ++i)
	{
		auto offset = i * SIDEDEF_SIZE;

		DoomSideDef side;
		side.offsetX = readInt16(sideDefs, offset);
		side.offsetY = readInt16(sideDefs, offset + 2);
		side.upperTexture = readName(sideDefs, offset + 4);
		side.lowerTexture = readName(sideDefs, offset + 12);
		side.middleTexture = readName(sideDefs, offset + 20);
		side.sector = readInt16(sideDefs, offset + 28);

		data.sideDefs.push_back(side);
	}

	data.lineDefs.reserve(lineCount);

	for (std::size_t i = 0; i < lineCount; ++i)
	{
		auto offset = i * lineSize;

		auto flags = readUInt16(lineDefs, offset + 4);

		DoomLineDef line;
		line.v1 = readUInt16(lineDefs, offset);
		line.v2 = readUInt16(lineDefs, offset + 2);
		line.dontPegTop = (flags & ML_DONTPEGTOP) != 0;
		line.dontPegBottom = (flags & ML_DONTPEGBOTTOM) != 0;

		auto sideOffset = offset + (isHexen ? 12 : 10);

		line.sideFront = toSideIndex(readUInt16(lineDefs, sideOffset));
		line.sideBack = toSideIndex(readUInt16(lineDefs, sideOffset + 2));

		data.lineDefs.push_back(line);
	}

	if (!things.empty())
	{
		auto thingSize = isHexen ? THING_SIZE_HEXEN : THING_SIZE;
		auto thingCount = things.size() / thingSize;

		data.things.reserve(thingCount);

		for (std::size_t i = 0; i < thingCount; ++i)
		{
			auto offset = i * thingSize;

			DoomThing thing;

			if (isHexen)
			{
				thing.x = readInt16(things, offset + 2);
				thing.y = readInt16(things, offset + 4);
				thing.angle = readInt16(things, offset + 8);
				thing.type = readInt16(things, offset + 10);
			}
			else
			{
				thing.x = readInt16(things, offset);
				thing.y = readInt16(things, offset + 2);
				thing.angle = readInt16(things, offset + 4);
				thing.type = readInt16(things, offset + 6);
			}

			data.things.push_back(thing);
		}
	}

	if (auto removed = removeInvalidLineDefs(data); removed > 0)
	{
		rWarning() << "[wad]: dropped " << removed << " linedefs with invalid vertices" << std::endl;
	}

	return data;
}

}

}
