#include "StrokeGeometry.h"

#include <algorithm>

namespace ui
{

namespace strokegeometry
{

namespace
{

double distanceToSegment(const Vector3& point, const Vector3& a, const Vector3& b)
{
    Vector3 direction = b - a;
    double lengthSquared = direction.dot(direction);

    if (lengthSquared < 1e-10)
    {
        return (point - a).getLength();
    }

    double t = std::clamp((point - a).dot(direction) / lengthSquared, 0.0, 1.0);

    return (point - (a + direction * t)).getLength();
}

void simplifySection(const std::vector<Vector3>& points, std::size_t first, std::size_t last,
    double tolerance, std::vector<bool>& keep)
{
    if (last <= first + 1)
    {
        return;
    }

    double maxDistance = 0;
    std::size_t maxIndex = first;

    for (std::size_t i = first + 1; i < last; ++i)
    {
        double distance = distanceToSegment(points[i], points[first], points[last]);

        if (distance > maxDistance)
        {
            maxDistance = distance;
            maxIndex = i;
        }
    }

    if (maxDistance <= tolerance)
    {
        return;
    }

    keep[maxIndex] = true;

    simplifySection(points, first, maxIndex, tolerance, keep);
    simplifySection(points, maxIndex, last, tolerance, keep);
}

}

std::vector<Vector3> simplify(const std::vector<Vector3>& points, double tolerance)
{
    if (points.size() < 3)
    {
        return points;
    }

    std::vector<bool> keep(points.size(), false);
    keep.front() = true;
    keep.back() = true;

    simplifySection(points, 0, points.size() - 1, tolerance, keep);

    std::vector<Vector3> result;

    for (std::size_t i = 0; i < points.size(); ++i)
    {
        if (keep[i])
        {
            result.push_back(points[i]);
        }
    }

    return result;
}

std::vector<Vector3> smooth(const std::vector<Vector3>& points, int iterations)
{
    if (points.size() < 3 || iterations <= 0)
    {
        return points;
    }

    std::vector<Vector3> current = points;

    for (int pass = 0; pass < iterations; ++pass)
    {
        std::vector<Vector3> next = current;

        for (std::size_t i = 1; i + 1 < current.size(); ++i)
        {
            next[i] = current[i - 1] * 0.25 + current[i] * 0.5 + current[i + 1] * 0.25;
        }

        current = std::move(next);
    }

    return current;
}

std::vector<Vector3> processStroke(const std::vector<Vector3>& points, bool smoothStroke, double tolerance)
{
    if (points.size() < 2)
    {
        return points;
    }

    std::vector<Vector3> result = points;

    if (smoothStroke)
    {
        result = smooth(result, 2);
        result = simplify(result, tolerance);
    }

    if (result.size() == 2)
    {
        result.insert(result.begin() + 1, (result[0] + result[1]) * 0.5);
    }

    return result;
}

}

}
