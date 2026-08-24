#include "WallGeometry.h"

#include "math/Plane3.h"
#include "math/Matrix3.h"
#include "math/pi.h"
#include "ui/building/BuildingGeometry.h"

#include <algorithm>
#include <cmath>
#include <map>

namespace ui
{

namespace wallgeometry
{

namespace
{

constexpr double Epsilon = 1e-6;
constexpr double JOINT_TOLERANCE = 0.01;

using PointKey = std::pair<long long, long long>;

PointKey endpointKey(const Vector2& point)
{
    return { std::llround(point.x() * 8.0), std::llround(point.y() * 8.0) };
}

Matrix3 defaultProjection()
{
    double texScale = building::getTextureScale();
    Matrix3 proj = Matrix3::getIdentity();
    proj.xx() = texScale;
    proj.yy() = texScale;
    return proj;
}

}

std::vector<polygon::Ring> trimFreeEnds(const std::vector<WallLine>& groupLines,
    const std::vector<WallLine>& levelLines)
{
    std::map<PointKey, int> degree;

    for (const WallLine& line : levelLines)
    {
        ++degree[endpointKey(line.a)];
        ++degree[endpointKey(line.b)];
    }

    auto isFree = [&degree](const Vector2& point)
    {
        auto found = degree.find(endpointKey(point));
        return found == degree.end() || found->second < 2;
    };

    std::vector<polygon::Ring> result;

    for (const WallLine& line : groupLines)
    {
        Vector2 direction = line.b - line.a;
        double length = direction.getLength();

        if (length < Epsilon)
        {
            result.push_back(polygon::Ring{ line.a, line.b });
            continue;
        }

        direction /= length;

        bool freeFrom = isFree(line.a);
        bool freeTo = isFree(line.b);
        double half = line.thickness * 0.5;
        double trim = (freeFrom ? half : 0.0) + (freeTo ? half : 0.0);

        if (trim >= length)
        {
            result.push_back(polygon::Ring{ line.a, line.b });
            continue;
        }

        result.push_back(polygon::Ring{
            freeFrom ? line.a + direction * half : line.a,
            freeTo ? line.b - direction * half : line.b });
    }

    return result;
}

std::vector<polygon::Ring> corridorMouths(const std::vector<WallLine>& levelLines)
{
    struct Stub
    {
        std::size_t host;
        Vector2 point;
        Vector2 away;
        double thickness;
        double along;
    };

    std::vector<Stub> stubs;

    for (std::size_t index = 0; index < levelLines.size(); ++index)
    {
        const WallLine& line = levelLines[index];

        for (int end = 0; end < 2; ++end)
        {
            const Vector2& point = end == 0 ? line.a : line.b;
            const Vector2& other = end == 0 ? line.b : line.a;

            Vector2 away = other - point;
            double length = away.getLength();

            if (length < Epsilon)
            {
                continue;
            }

            away /= length;

            for (std::size_t h = 0; h < levelLines.size(); ++h)
            {
                if (h == index)
                {
                    continue;
                }

                const WallLine& host = levelLines[h];

                Vector2 span = host.b - host.a;
                double hostLength = span.getLength();

                if (hostLength < Epsilon)
                {
                    continue;
                }

                Vector2 direction = span / hostLength;
                double along = (point - host.a).dot(direction);

                if (along <= Epsilon || along >= hostLength - Epsilon)
                {
                    continue;
                }

                if (std::abs((point - host.a).crossProduct(direction)) > JOINT_TOLERANCE)
                {
                    continue;
                }

                stubs.push_back({ h, point, away, line.thickness, along });
                break;
            }
        }
    }

    std::map<std::size_t, std::vector<Stub>> byHost;

    for (const Stub& stub : stubs)
    {
        byHost[stub.host].push_back(stub);
    }

    std::vector<polygon::Ring> mouths;

    for (auto& entry : byHost)
    {
        std::vector<Stub>& group = entry.second;

        std::sort(group.begin(), group.end(),
            [](const Stub& left, const Stub& right) { return left.along < right.along; });

        const WallLine& host = levelLines[entry.first];

        std::vector<polygon::Ring> hostFootprint =
            polygon::thicken({ polygon::Ring{ host.a, host.b } }, host.thickness);

        double reach = (host.b - host.a).getLength() + host.thickness;

        for (std::size_t i = 0; i + 1 < group.size(); ++i)
        {
            const Stub& first = group[i];
            const Stub& second = group[i + 1];

            if (std::abs(first.away.crossProduct(second.away)) > JOINT_TOLERANCE)
            {
                continue;
            }

            if (first.away.dot(second.away) <= 0)
            {
                continue;
            }

            Vector2 across = second.point - first.point;
            double gap = across.getLength();

            if (gap < Epsilon)
            {
                continue;
            }

            across /= gap;

            Vector2 from = first.point + across * (first.thickness * 0.5);
            Vector2 to = second.point - across * (second.thickness * 0.5);

            if ((to - from).dot(across) <= Epsilon)
            {
                continue;
            }

            Vector2 depth = first.away * reach;

            polygon::Ring channel{ from - depth, to - depth, to + depth, from + depth };

            std::vector<polygon::Ring> cut = polygon::intersect({ channel }, hostFootprint);

            mouths.insert(mouths.end(), cut.begin(), cut.end());
        }
    }

    return mouths;
}

std::vector<polygon::Ring> levelFootprint(const std::vector<WallLine>& levelLines)
{
    std::map<long long, std::vector<WallLine>> byThickness;

    for (const WallLine& line : levelLines)
    {
        byThickness[std::llround(line.thickness * 8.0)].push_back(line);
    }

    std::vector<polygon::Ring> footprint;

    for (const auto& entry : byThickness)
    {
        std::vector<polygon::Ring> thickened = polygon::thicken(
            trimFreeEnds(entry.second, levelLines), entry.second.front().thickness);

        footprint.insert(footprint.end(), thickened.begin(), thickened.end());
    }

    return footprint;
}

std::vector<polygon::Region> walkableRegions(const std::vector<WallLine>& levelLines,
    const std::vector<polygon::Ring>& mouths)
{
    std::vector<polygon::Ring> rooms;

    for (const polygon::Region& enclosure : polygon::regions(levelFootprint(levelLines)))
    {
        rooms.insert(rooms.end(), enclosure.holes.begin(), enclosure.holes.end());
    }

    for (polygon::Ring& ring : rooms)
    {
        if (polygon::ringArea(ring) < 0)
        {
            std::reverse(ring.begin(), ring.end());
        }
    }

    if (rooms.empty())
    {
        return {};
    }

    std::vector<polygon::Ring> walkable = rooms;

    for (const polygon::Ring& mouth : mouths)
    {
        walkable.push_back(mouth);

        if (polygon::ringArea(walkable.back()) < 0)
        {
            std::reverse(walkable.back().begin(), walkable.back().end());
        }
    }

    std::vector<polygon::Region> result;

    for (const polygon::Region& region : polygon::regions(walkable))
    {
        if (!polygon::intersect({ region.outer }, rooms).empty())
        {
            result.push_back(region);
        }
    }

    return result;
}

Vector2 snapSegmentEnd(const Vector2& anchor, const Vector2& current, double gridSize)
{
    Vector2 drag = current - anchor;

    if (drag.getLengthSquared() < Epsilon)
    {
        return anchor;
    }

    static const int directions[8][2] = {
        { 1, 0 }, { 1, 1 }, { 0, 1 }, { -1, 1 }, { -1, 0 }, { -1, -1 }, { 0, -1 }, { 1, -1 }
    };

    double angle = std::atan2(drag.y(), drag.x());
    int octant = static_cast<int>(std::lround(angle / (math::PI / 4.0)));
    octant = ((octant % 8) + 8) % 8;

    Vector2 dir(directions[octant][0], directions[octant][1]);
    bool diagonal = directions[octant][0] != 0 && directions[octant][1] != 0;

    double step = diagonal ? gridSize * std::sqrt(2.0) : gridSize;
    long units = std::lround(drag.dot(dir.getNormalised()) / step);

    if (units <= 0)
    {
        return anchor;
    }

    return Vector2(anchor.x() + directions[octant][0] * units * gridSize,
        anchor.y() + directions[octant][1] * units * gridSize);
}

void buildWallSegmentFaces(IBrush& brush, const Vector2& a, const Vector2& b,
    double baseZ, double height, double thickness, const std::string& material)
{
    Vector2 dir = (b - a).getNormalised();
    Vector2 perp(-dir.y(), dir.x());
    double half = thickness * 0.5;

    Matrix3 proj = defaultProjection();

    brush.clear();
    brush.addFace(Plane3(0, 0, 1, baseZ + height), proj, material);
    brush.addFace(Plane3(0, 0, -1, -baseZ), proj, material);
    brush.addFace(Plane3(perp.x(), perp.y(), 0, perp.dot(a) + half), proj, material);
    brush.addFace(Plane3(-perp.x(), -perp.y(), 0, -perp.dot(a) + half), proj, material);
    brush.addFace(Plane3(dir.x(), dir.y(), 0, dir.dot(b)), proj, material);
    brush.addFace(Plane3(-dir.x(), -dir.y(), 0, -dir.dot(a)), proj, material);

    brush.evaluateBRep();
}

double distanceToSegment(const Vector2& point, const Vector2& a, const Vector2& b)
{
    Vector2 delta = b - a;
    double lengthSquared = delta.getLengthSquared();

    if (lengthSquared < Epsilon)
    {
        return (point - a).getLength();
    }

    double t = std::max(0.0, std::min(1.0, (point - a).dot(delta) / lengthSquared));

    return (point - (a + delta * t)).getLength();
}

void buildPrismFaces(IBrush& brush, const std::vector<Vector2>& polygon,
    double zBottom, double zTop, const std::string& material)
{
    Matrix3 proj = defaultProjection();

    brush.clear();
    brush.addFace(Plane3(0, 0, 1, zTop), proj, material);
    brush.addFace(Plane3(0, 0, -1, -zBottom), proj, material);

    for (std::size_t i = 0; i < polygon.size(); ++i)
    {
        const Vector2& p = polygon[i];
        const Vector2& q = polygon[(i + 1) % polygon.size()];

        Vector2 edge = q - p;
        double length = edge.getLength();

        if (length < Epsilon)
        {
            continue;
        }

        Vector2 normal(edge.y() / length, -edge.x() / length);
        brush.addFace(Plane3(normal.x(), normal.y(), 0, normal.dot(p)), proj, material);
    }

    brush.evaluateBRep();
}

void buildSlopedPrismFaces(IBrush& brush, const std::vector<Vector2>& polygon,
    double zBottom, const Plane3& top, const std::string& material)
{
    Matrix3 proj = defaultProjection();

    brush.clear();
    brush.addFace(top, proj, material);
    brush.addFace(Plane3(0, 0, -1, -zBottom), proj, material);

    for (std::size_t i = 0; i < polygon.size(); ++i)
    {
        const Vector2& p = polygon[i];
        const Vector2& q = polygon[(i + 1) % polygon.size()];

        Vector2 edge = q - p;
        double length = edge.getLength();

        if (length < Epsilon)
        {
            continue;
        }

        Vector2 normal(edge.y() / length, -edge.x() / length);
        brush.addFace(Plane3(normal.x(), normal.y(), 0, normal.dot(p)), proj, material);
    }

    brush.evaluateBRep();
}

}

}
