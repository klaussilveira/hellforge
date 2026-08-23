#include "RoadNetwork.h"

#include "clipper2/clipper.h"
#include "math/curve.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <map>

namespace road
{

namespace
{

const double LINE_EPSILON = 0.000000001;
const double WELD_TOLERANCE = 0.01;
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

Clipper2Lib::PathsD ringPaths(const std::vector<Ring>& rings)
{
    Clipper2Lib::PathsD paths;

    for (const Ring& ring : rings)
    {
        Clipper2Lib::PathD path;

        for (const Vector2& point : ring)
        {
            path.push_back(Clipper2Lib::PointD(point.x(), point.y()));
        }

        paths.push_back(path);
    }

    return paths;
}

std::vector<Ring> pathRings(const Clipper2Lib::PathsD& paths)
{
    std::vector<Ring> rings;

    for (const Clipper2Lib::PathD& path : paths)
    {
        if (path.size() < 3)
        {
            continue;
        }

        Ring ring;

        for (const Clipper2Lib::PointD& point : path)
        {
            ring.push_back(Vector2(point.x, point.y));
        }

        rings.push_back(ring);
    }

    return rings;
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

long long quantise(double value)
{
    return static_cast<long long>(std::llround(value / WELD_TOLERANCE));
}

typedef std::array<long long, 4> EdgeKey;

EdgeKey edgeKey(const Vector2& from, const Vector2& to)
{
    return EdgeKey{ quantise(from.x()), quantise(from.y()), quantise(to.x()), quantise(to.y()) };
}

void makeCounterClockwise(Ring& ring)
{
    if (ringArea(ring) < 0)
    {
        std::reverse(ring.begin(), ring.end());
    }
}

Ring mergedRing(const Ring& first, std::size_t firstEdge, const Ring& second,
                std::size_t secondEdge)
{
    Ring result;
    result.push_back(first[firstEdge]);

    for (std::size_t step = 0; step + 1 < second.size(); ++step)
    {
        result.push_back(second[(secondEdge + 2 + step) % second.size()]);
    }

    for (std::size_t step = 0; step + 2 < first.size(); ++step)
    {
        result.push_back(first[(firstEdge + 2 + step) % first.size()]);
    }

    return result;
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

double ringArea(const Ring& ring)
{
    double total = 0;

    for (std::size_t index = 0; index < ring.size(); ++index)
    {
        const Vector2& current = ring[index];
        const Vector2& next = ring[(index + 1) % ring.size()];

        total += current.crossProduct(next);
    }

    return total * 0.5;
}

bool isConvex(const Ring& ring)
{
    if (ring.size() < 3)
    {
        return false;
    }

    bool positive = false;
    bool negative = false;

    for (std::size_t index = 0; index < ring.size(); ++index)
    {
        Vector2 current = ring[(index + 1) % ring.size()] - ring[index];
        Vector2 next = ring[(index + 2) % ring.size()] - ring[(index + 1) % ring.size()];

        double turn = current.crossProduct(next);

        if (turn > POSITION_EPSILON)
        {
            positive = true;
        }

        if (turn < -POSITION_EPSILON)
        {
            negative = true;
        }
    }

    return !(positive && negative);
}

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

    footprint.carriage = pathRings(carriage);

    if (sidewalkWidth <= 0)
    {
        return footprint;
    }

    Clipper2Lib::PathsD corridor =
        inflate(carriage, sidewalkWidth, Clipper2Lib::EndType::Polygon);

    footprint.band =
        pathRings(Clipper2Lib::Difference(corridor, carriage, Clipper2Lib::FillRule::NonZero,
                                          CLIPPER_PRECISION));

    return footprint;
}

std::vector<Ring> triangulate(const std::vector<Ring>& rings)
{
    std::vector<Ring> result;

    if (rings.empty())
    {
        return result;
    }

    Clipper2Lib::PathsD solution;

    if (Clipper2Lib::Triangulate(ringPaths(rings), CLIPPER_PRECISION, solution, true) !=
        Clipper2Lib::TriangulateResult::success)
    {
        return result;
    }

    return pathRings(solution);
}

std::vector<Ring> mergeConvex(const std::vector<Ring>& pieces,
                              const std::function<bool(const Ring&)>& accept)
{
    std::vector<Ring> result = pieces;

    for (Ring& ring : result)
    {
        makeCounterClockwise(ring);
    }

    bool changed = true;

    while (changed)
    {
        changed = false;

        std::map<EdgeKey, std::pair<std::size_t, std::size_t>> edges;

        for (std::size_t piece = 0; piece < result.size(); ++piece)
        {
            for (std::size_t edge = 0; edge < result[piece].size(); ++edge)
            {
                const Ring& ring = result[piece];
                EdgeKey key = edgeKey(ring[edge], ring[(edge + 1) % ring.size()]);

                edges.emplace(key, std::make_pair(piece, edge));
            }
        }

        std::vector<bool> locked(result.size(), false);

        for (const auto& entry : edges)
        {
            std::size_t first = entry.second.first;
            std::size_t firstEdge = entry.second.second;

            if (locked[first] || result[first].empty())
            {
                continue;
            }

            EdgeKey reverse = EdgeKey{ entry.first[2], entry.first[3], entry.first[0],
                                       entry.first[1] };

            auto neighbour = edges.find(reverse);

            if (neighbour == edges.end())
            {
                continue;
            }

            std::size_t second = neighbour->second.first;
            std::size_t secondEdge = neighbour->second.second;

            if (second == first || locked[second] || result[second].empty())
            {
                continue;
            }

            Ring candidate =
                mergedRing(result[first], firstEdge, result[second], secondEdge);

            if (!isConvex(candidate) || !accept(candidate))
            {
                continue;
            }

            result[first] = candidate;
            result[second].clear();
            locked[first] = true;
            locked[second] = true;
            changed = true;
        }

        result.erase(std::remove_if(result.begin(), result.end(),
                                    [](const Ring& ring) { return ring.size() < 3; }),
                     result.end());
    }

    return result;
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
