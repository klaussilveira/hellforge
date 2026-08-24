#pragma once

#include "ibrush.h"
#include "math/Plane3.h"
#include "math/Vector2.h"
#include "polygon/Polygon2D.h"
#include <string>
#include <vector>

namespace ui
{

namespace wallgeometry
{

Vector2 snapSegmentEnd(const Vector2& anchor, const Vector2& current, double gridSize);

struct WallLine
{
    Vector2 a;
    Vector2 b;
    double thickness;
};

std::vector<polygon::Ring> trimFreeEnds(const std::vector<WallLine>& groupLines,
    const std::vector<WallLine>& levelLines);

std::vector<polygon::Ring> corridorMouths(const std::vector<WallLine>& levelLines);

std::vector<polygon::Ring> levelFootprint(const std::vector<WallLine>& levelLines);

std::vector<polygon::Region> walkableRegions(const std::vector<WallLine>& levelLines,
    const std::vector<polygon::Ring>& mouths);

void buildWallSegmentFaces(IBrush& brush, const Vector2& a, const Vector2& b,
    double baseZ, double height, double thickness, const std::string& material);

double distanceToSegment(const Vector2& point, const Vector2& a, const Vector2& b);

void buildPrismFaces(IBrush& brush, const std::vector<Vector2>& polygon,
    double zBottom, double zTop, const std::string& material);

void buildSlopedPrismFaces(IBrush& brush, const std::vector<Vector2>& polygon,
    double zBottom, const Plane3& top, const std::string& material);

}

}
