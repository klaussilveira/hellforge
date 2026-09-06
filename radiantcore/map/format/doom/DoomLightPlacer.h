#pragma once

#include "DoomMapData.h"
#include "SectorGeometry.h"

#include "math/Vector2.h"

#include <vector>

namespace map
{

namespace doom
{

struct DoomLight
{
	Vector2 position;
	double height = 0;
	double radius = 0;
	int lightLevel = 160;
};

class DoomLightPlacer
{
public:
	static std::vector<DoomLight> place(const DoomMapData& data,
		const std::vector<SectorFootprint>& footprints, double spacing);
};

}

}
