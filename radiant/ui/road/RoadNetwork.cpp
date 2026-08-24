#include "RoadNetwork.h"

#include "clipper2/clipper.h"
#include "math/curve.h"

#include <algorithm>
#include <cmath>

namespace road
{

namespace
{

const double LINE_EPSILON = 0.000000001;
const double SIMPLIFY_EPSILON = 0.5;

Vector2 flatten(const Vector3& point)
{
    return Vector2(point.x(), point.y());
}

void removeDuplicatePoints(std::vector<Vector3>& points)
{
    std::vector<Vector3> cleaned;

    for (const Vector3& point : points)
    {
        if (cleaned.empty() ||
            (flatten(point) - flatten(cleaned.back())).getLength() > POSITION_EPSILON)
        {
            cleaned.push_back(point);
        }
    }

    points.swap(cleaned);
}

Clipper2Lib::PathsD centrelinePaths(const std::vector<std::vector<Vector3>>& centrelines)
{
    Clipper2Lib::PathsD paths;

    for (const std::vector<Vector3>& line : centrelines)
    {
        if (line.size() < 2)
        {
            continue;
        }

        Clipper2Lib::PathD path;

        for (const Vector3& point : line)
        {
            path.push_back(Clipper2Lib::PointD(point.x(), point.y()));
        }

        paths.push_back(path);
    }

    return paths;
}

Clipper2Lib::PathsD inflate(const Clipper2Lib::PathsD& paths, double delta,
                            Clipper2Lib::EndType endType)
{
    if (paths.empty() || std::fabs(delta) < LINE_EPSILON)
    {
        return paths;
    }

    Clipper2Lib::PathsD grown =
        Clipper2Lib::InflatePaths(paths, delta, Clipper2Lib::JoinType::Round, endType, 2.0,
                                  CLIPPER_PRECISION, ARC_TOLERANCE);

    return Clipper2Lib::Union(grown, Clipper2Lib::FillRule::NonZero);
}

Clipper2Lib::PathsD tidy(const Clipper2Lib::PathsD& paths)
{
    return Clipper2Lib::SimplifyPaths(paths, SIMPLIFY_EPSILON);
}

Clipper2Lib::PathsD closeCorners(const Clipper2Lib::PathsD& paths, double radius)
{
    if (radius <= 0)
    {
        return paths;
    }

    Clipper2Lib::PathsD grown = inflate(paths, radius, Clipper2Lib::EndType::Polygon);
    Clipper2Lib::PathsD shrunk = inflate(grown, -radius, Clipper2Lib::EndType::Polygon);

    shrunk.insert(shrunk.end(), paths.begin(), paths.end());

    return Clipper2Lib::Union(shrunk, Clipper2Lib::FillRule::NonZero);
}

double segmentDistance(const Vector2& point, const Vector2& from, const Vector2& to)
{
    Vector2 step = to - from;
    double lengthSquared = step.dot(step);

    if (lengthSquared < LINE_EPSILON)
    {
        return (point - from).getLength();
    }

    double parameter = (point - from).dot(step) / lengthSquared;
    parameter = std::max(0.0, std::min(1.0, parameter));

    return (point - (from + step * parameter)).getLength();
}

} // anonymous namespace

std::vector<Vector3> sampleCurve(const std::vector<Vector3>& controlPoints, int samples)
{
    std::vector<Vector3> result;

    if (controlPoints.size() < 3 || samples < 1)
    {
        return result;
    }

    int total = samples * static_cast<int>(controlPoints.size() - 1);

    for (int step = 0; step <= total; ++step)
    {
        result.push_back(CatmullRom_evaluate(controlPoints, static_cast<double>(step) / total));
    }

    removeDuplicatePoints(result);

    return result;
}

Footprint buildFootprint(const std::vector<std::vector<Vector3>>& centrelines, double roadHalf,
                         double sidewalkWidth, double cornerRadius, bool roundCorners)
{
    Footprint footprint;

    Clipper2Lib::PathsD lines = centrelinePaths(centrelines);

    if (lines.empty() || roadHalf <= 0)
    {
        return footprint;
    }

    double radius = roundCorners ? cornerRadius : 0;

    Clipper2Lib::PathsD carriage = inflate(lines, roadHalf, Clipper2Lib::EndType::Butt);
    carriage = tidy(closeCorners(carriage, radius));

    footprint.carriage = polygon::toRings(carriage);

    if (sidewalkWidth <= 0)
    {
        return footprint;
    }

    Clipper2Lib::PathsD corridor =
        inflate(carriage, sidewalkWidth, Clipper2Lib::EndType::Polygon);

    footprint.band = polygon::toRings(
        Clipper2Lib::Difference(corridor, carriage, Clipper2Lib::FillRule::NonZero,
                                CLIPPER_PRECISION));

    return footprint;
}

bool onBoundary(const std::vector<Ring>& rings, const Vector2& point, double tolerance)
{
    for (const Ring& ring : rings)
    {
        for (std::size_t index = 0; index < ring.size(); ++index)
        {
            if (segmentDistance(point, ring[index], ring[(index + 1) % ring.size()]) <= tolerance)
            {
                return true;
            }
        }
    }

    return false;
}

Anchor anchorAt(const std::vector<std::vector<Vector3>>& centrelines, const Vector2& point)
{
    Anchor anchor;
    double best = -1;

    for (std::size_t index = 0; index < centrelines.size(); ++index)
    {
        const std::vector<Vector3>& line = centrelines[index];
        double travelled = 0;

        for (std::size_t segment = 0; segment + 1 < line.size(); ++segment)
        {
            const Vector3& from = line[segment];
            const Vector3& to = line[segment + 1];

            Vector2 step = flatten(to) - flatten(from);
            double length = step.getLength();

            if (length < LINE_EPSILON)
            {
                continue;
            }

            double parameter = (point - flatten(from)).dot(step) / (length * length);
            parameter = std::max(0.0, std::min(1.0, parameter));

            Vector3 candidate = from + (to - from) * parameter;
            double distance = (point - flatten(candidate)).getLength();

            if (best < 0 || distance < best)
            {
                best = distance;
                anchor.position = candidate;
                anchor.direction = (to - from).getNormalised();
                anchor.travelled = travelled + length * parameter;
                anchor.line = index;
            }

            travelled += length;
        }
    }

    return anchor;
}

} // namespace road
