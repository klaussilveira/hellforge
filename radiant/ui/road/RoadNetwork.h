#pragma once

#include "math/Vector2.h"
#include "math/Vector3.h"

#include <cstddef>
#include <functional>
#include <vector>

namespace road
{

const double POSITION_EPSILON = 0.01;
const int CLIPPER_PRECISION = 2;
const double ARC_TOLERANCE = 1.0;

typedef std::vector<Vector2> Ring;

struct Footprint
{
    std::vector<Ring> carriage;
    std::vector<Ring> band;
};

struct Anchor
{
    Vector3 position = Vector3(0, 0, 0);
    Vector3 direction = Vector3(1, 0, 0);
    double travelled = 0;
    std::size_t line = 0;
};

std::vector<Vector3> sampleCurve(const std::vector<Vector3>& controlPoints, int samples);

Footprint buildFootprint(const std::vector<std::vector<Vector3>>& centrelines, double roadHalf,
                         double sidewalkWidth, double cornerRadius, bool roundCorners);

std::vector<Ring> triangulate(const std::vector<Ring>& rings);

std::vector<Ring> mergeConvex(const std::vector<Ring>& pieces,
                              const std::function<bool(const Ring&)>& accept);

bool onBoundary(const std::vector<Ring>& rings, const Vector2& point, double tolerance);

Anchor anchorAt(const std::vector<std::vector<Vector3>>& centrelines, const Vector2& point);

double ringArea(const Ring& ring);

bool isConvex(const Ring& ring);

} // namespace road
