#pragma once

#include "math/Vector3.h"

#include <vector>

namespace ui
{

namespace strokegeometry
{

std::vector<Vector3> simplify(const std::vector<Vector3>& points, double tolerance);

std::vector<Vector3> smooth(const std::vector<Vector3>& points, int iterations);

std::vector<Vector3> processStroke(const std::vector<Vector3>& points, bool smoothStroke, double tolerance);

}

}
