#pragma once

#include <algorithm>
#include <cstddef>
#include <iterator>
#include <string>
#include <vector>

namespace map
{

namespace doom
{

const int NO_INDEX = -1;

const char* const NO_TEXTURE = "-";

struct DoomVertex
{
	double x = 0;
	double y = 0;
};

struct DoomSector
{
	double floorHeight = 0;
	double ceilingHeight = 0;
	std::string floorTexture;
	std::string ceilingTexture;
	int lightLevel = 160;
};

struct DoomSideDef
{
	double offsetX = 0;
	double offsetY = 0;
	std::string upperTexture;
	std::string lowerTexture;
	std::string middleTexture;
	int sector = NO_INDEX;
};

struct DoomLineDef
{
	int v1 = NO_INDEX;
	int v2 = NO_INDEX;
	int sideFront = NO_INDEX;
	int sideBack = NO_INDEX;
	bool dontPegTop = false;
	bool dontPegBottom = false;
};

struct DoomThing
{
	double x = 0;
	double y = 0;
	double angle = 0;
	int type = 0;
};

struct DoomMapData
{
	std::vector<DoomVertex> vertices;
	std::vector<DoomSector> sectors;
	std::vector<DoomSideDef> sideDefs;
	std::vector<DoomLineDef> lineDefs;
	std::vector<DoomThing> things;

	int getSectorOfSide(int sideIndex) const
	{
		if (sideIndex < 0 || static_cast<std::size_t>(sideIndex) >= sideDefs.size())
		{
			return NO_INDEX;
		}

		auto sector = sideDefs[sideIndex].sector;

		return sector >= 0 && static_cast<std::size_t>(sector) < sectors.size() ? sector : NO_INDEX;
	}
};

inline bool textureIsSet(const std::string& name)
{
	return !name.empty() && name != NO_TEXTURE;
}

inline std::size_t removeInvalidLineDefs(DoomMapData& data)
{
	auto vertexCount = static_cast<int>(data.vertices.size());

	auto removed = std::remove_if(data.lineDefs.begin(), data.lineDefs.end(),
		[vertexCount](const DoomLineDef& line)
		{
			return line.v1 < 0 || line.v1 >= vertexCount ||
				line.v2 < 0 || line.v2 >= vertexCount || line.v1 == line.v2;
		});

	auto count = static_cast<std::size_t>(std::distance(removed, data.lineDefs.end()));

	data.lineDefs.erase(removed, data.lineDefs.end());

	return count;
}

}

}
