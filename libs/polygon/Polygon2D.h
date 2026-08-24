#pragma once

#include "clipper2/clipper.h"
#include "math/Vector2.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <functional>
#include <map>
#include <vector>

namespace polygon
{

typedef std::vector<Vector2> Ring;

const int PRECISION = 2;
const double EPSILON = 0.01;
const double MITER_LIMIT = 4.0;

struct Region
{
    Ring outer;
    std::vector<Ring> holes;
};

inline double ringArea(const Ring& ring)
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

inline bool isConvex(const Ring& ring)
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

        if (turn > EPSILON)
        {
            positive = true;
        }

        if (turn < -EPSILON)
        {
            negative = true;
        }
    }

    return !(positive && negative);
}

inline Clipper2Lib::PathsD toPaths(const std::vector<Ring>& rings)
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

namespace detail
{

inline Ring pathRing(const Clipper2Lib::PathD& path)
{
    Ring ring;

    for (const Clipper2Lib::PointD& point : path)
    {
        ring.push_back(Vector2(point.x, point.y));
    }

    return ring;
}

} // namespace detail

inline std::vector<Ring> toRings(const Clipper2Lib::PathsD& paths)
{
    std::vector<Ring> rings;

    for (const Clipper2Lib::PathD& path : paths)
    {
        if (path.size() >= 3)
        {
            rings.push_back(detail::pathRing(path));
        }
    }

    return rings;
}

inline std::vector<Ring> repair(const std::vector<Ring>& rings, double simplifyEpsilon)
{
    Clipper2Lib::PathsD paths = toPaths(rings);

    if (simplifyEpsilon > 0)
    {
        paths = Clipper2Lib::SimplifyPaths(paths, simplifyEpsilon);
    }

    return toRings(Clipper2Lib::Union(paths, Clipper2Lib::FillRule::NonZero, PRECISION));
}

inline std::vector<Ring> intersect(const std::vector<Ring>& subject,
                                   const std::vector<Ring>& clip)
{
    if (subject.empty() || clip.empty())
    {
        return {};
    }

    return toRings(Clipper2Lib::Intersect(toPaths(subject), toPaths(clip),
                                          Clipper2Lib::FillRule::NonZero, PRECISION));
}

inline std::vector<Ring> difference(const std::vector<Ring>& subject,
                                    const std::vector<Ring>& clip)
{
    if (subject.empty() || clip.empty())
    {
        return subject;
    }

    return toRings(Clipper2Lib::Difference(toPaths(subject), toPaths(clip),
                                           Clipper2Lib::FillRule::NonZero, PRECISION));
}

inline std::vector<Ring> triangulate(const std::vector<Ring>& rings)
{
    std::vector<Ring> result;

    if (rings.empty())
    {
        return result;
    }

    Clipper2Lib::PathsD solution;

    if (Clipper2Lib::Triangulate(toPaths(rings), PRECISION, solution, true) !=
        Clipper2Lib::TriangulateResult::success)
    {
        return result;
    }

    return toRings(solution);
}

inline std::vector<Ring> thicken(const std::vector<Ring>& centrelines, double width)
{
    if (centrelines.empty() || width <= 0)
    {
        return {};
    }

    Clipper2Lib::PathsD grown = Clipper2Lib::InflatePaths(
        toPaths(centrelines), width * 0.5, Clipper2Lib::JoinType::Miter,
        Clipper2Lib::EndType::Square, MITER_LIMIT, PRECISION);

    return toRings(Clipper2Lib::Union(grown, Clipper2Lib::FillRule::NonZero, PRECISION));
}

namespace detail
{

const double WELD_TOLERANCE = 0.01;

inline void collectRegions(const Clipper2Lib::PolyPathD& parent, std::vector<Region>& result)
{
    for (std::size_t index = 0; index < parent.Count(); ++index)
    {
        const Clipper2Lib::PolyPathD& outer = *parent[index];

        Region region;
        region.outer = pathRing(outer.Polygon());

        for (std::size_t hole = 0; hole < outer.Count(); ++hole)
        {
            const Clipper2Lib::PolyPathD& inner = *outer[hole];

            if (inner.Polygon().size() >= 3)
            {
                region.holes.push_back(pathRing(inner.Polygon()));
            }

            collectRegions(inner, result);
        }

        if (region.outer.size() >= 3)
        {
            result.push_back(region);
        }
    }
}


typedef std::array<long long, 4> EdgeKey;

inline long long quantise(double value)
{
    return static_cast<long long>(std::llround(value / WELD_TOLERANCE));
}

inline EdgeKey edgeKey(const Vector2& from, const Vector2& to)
{
    return EdgeKey{ quantise(from.x()), quantise(from.y()), quantise(to.x()), quantise(to.y()) };
}

inline void makeCounterClockwise(Ring& ring)
{
    if (ringArea(ring) < 0)
    {
        std::reverse(ring.begin(), ring.end());
    }
}

inline Ring dropCollinear(const Ring& ring)
{
    Ring cleaned;

    for (const Vector2& point : ring)
    {
        if (cleaned.empty() || (point - cleaned.back()).getLength() > EPSILON)
        {
            cleaned.push_back(point);
        }
    }

    while (cleaned.size() > 1 && (cleaned.front() - cleaned.back()).getLength() <= EPSILON)
    {
        cleaned.pop_back();
    }

    if (cleaned.size() < 3)
    {
        return Ring();
    }

    Ring result;

    for (std::size_t index = 0; index < cleaned.size(); ++index)
    {
        const Vector2& previous = cleaned[(index + cleaned.size() - 1) % cleaned.size()];
        const Vector2& current = cleaned[index];
        const Vector2& next = cleaned[(index + 1) % cleaned.size()];

        Vector2 span = next - previous;
        double length = span.getLength();

        if (length <= EPSILON)
        {
            continue;
        }

        if (std::fabs(span.crossProduct(current - previous)) / length <= EPSILON)
        {
            continue;
        }

        result.push_back(current);
    }

    return result.size() >= 3 ? result : Ring();
}

inline Ring mergedRing(const Ring& first, std::size_t firstEdge, const Ring& second,
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

} // namespace detail

inline std::vector<Ring> mergeConvex(const std::vector<Ring>& pieces,
                                     const std::function<bool(const Ring&)>& accept)
{
    std::vector<Ring> result = pieces;

    for (Ring& ring : result)
    {
        detail::makeCounterClockwise(ring);
    }

    bool changed = true;

    while (changed)
    {
        changed = false;

        std::map<detail::EdgeKey, std::pair<std::size_t, std::size_t>> edges;

        for (std::size_t piece = 0; piece < result.size(); ++piece)
        {
            for (std::size_t edge = 0; edge < result[piece].size(); ++edge)
            {
                const Ring& ring = result[piece];
                detail::EdgeKey key =
                    detail::edgeKey(ring[edge], ring[(edge + 1) % ring.size()]);

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

            detail::EdgeKey reverse = detail::EdgeKey{ entry.first[2], entry.first[3],
                                                       entry.first[0], entry.first[1] };

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
                detail::mergedRing(result[first], firstEdge, result[second], secondEdge);

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

    for (Ring& ring : result)
    {
        ring = detail::dropCollinear(ring);
    }

    result.erase(std::remove_if(result.begin(), result.end(),
                                [](const Ring& ring) { return ring.size() < 3; }),
                 result.end());

    return result;
}

inline std::vector<Region> regions(const std::vector<Ring>& rings)
{
    std::vector<Region> result;

    if (rings.empty())
    {
        return result;
    }

    Clipper2Lib::PolyTreeD tree;

    Clipper2Lib::BooleanOp(Clipper2Lib::ClipType::Union, Clipper2Lib::FillRule::NonZero,
                           toPaths(rings), Clipper2Lib::PathsD(), tree, PRECISION);

    detail::collectRegions(tree, result);

    return result;
}

inline std::vector<Ring> convexPieces(const std::vector<Ring>& rings, double simplifyEpsilon)
{
    return mergeConvex(triangulate(repair(rings, simplifyEpsilon)),
                       [](const Ring&) { return true; });
}

} // namespace polygon
