#pragma once

#include "DoomMapData.h"

#include "polygon/Polygon2D.h"
#include "math/Vector2.h"

#include <vector>

namespace map
{

namespace doom
{

struct SectorFootprint
{
	std::vector<polygon::Ring> rings;
	std::vector<polygon::Ring> pieces;

	Vector2 boundsMin;
	Vector2 boundsMax;

	double prismBottom = 0;
	double prismTop = 0;

	bool valid = false;
};

class SectorGeometry
{
public:
	static std::vector<SectorFootprint> build(const DoomMapData& data, double prismMargin);
	static std::vector<polygon::Ring> buildShell(const std::vector<SectorFootprint>& footprints,
		double thickness);
};

}

}
