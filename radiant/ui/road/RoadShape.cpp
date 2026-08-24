#include "RoadShape.h"

#include "math/pi.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <map>

namespace road
{

namespace
{

const double BASE_DEPTH = 16.0;
const double MIN_AREA = 4.0;
const double LINE_EPSILON = 0.000000001;
const double COPLANAR_EPSILON = 0.05;
const double BOUNDARY_EPSILON = 0.05;
const double PLANE_QUANTUM = 0.0001;
const std::size_t CURB_FACETS = 3;

double curbFilletRadius(const RoadParams& params)
{
    double radius = params.curbRadius;

    radius = std::min(radius, params.sidewalkHeight);
    radius = std::min(radius, params.sidewalkWidth);

    return std::max(0.0, radius);
}

Vector2 flatten(const Vector3& point)
{
    return Vector2(point.x(), point.y());
}

Vector3 lift(const Vector2& point, double z)
{
    return Vector3(point.x(), point.y(), z);
}

Vector3 centroidOf(const std::vector<Vector3>& points)
{
    Vector3 total(0, 0, 0);

    for (const Vector3& point : points)
    {
        total += point;
    }

    return total / static_cast<double>(points.size());
}

Plane3 orientedPlane(const Vector3& normal, const Vector3& through, const Vector3& interior)
{
    Plane3 plane(normal, normal.dot(through));

    if (plane.normal().dot(interior) > plane.dist())
    {
        return -plane;
    }

    return plane;
}

bool ringPlane(const std::vector<Vector3>& points, const Vector3& interior, Plane3& result)
{
    double sumXX = 0, sumXY = 0, sumYY = 0, sumX = 0, sumY = 0;
    double sumXZ = 0, sumYZ = 0, sumZ = 0;
    double count = static_cast<double>(points.size());

    for (const Vector3& point : points)
    {
        sumXX += point.x() * point.x();
        sumXY += point.x() * point.y();
        sumYY += point.y() * point.y();
        sumX += point.x();
        sumY += point.y();
        sumXZ += point.x() * point.z();
        sumYZ += point.y() * point.z();
        sumZ += point.z();
    }

    double determinant = sumXX * (sumYY * count - sumY * sumY) -
                         sumXY * (sumXY * count - sumY * sumX) +
                         sumX * (sumXY * sumY - sumYY * sumX);

    if (std::fabs(determinant) < LINE_EPSILON)
    {
        return false;
    }

    double slopeX = (sumXZ * (sumYY * count - sumY * sumY) -
                     sumXY * (sumYZ * count - sumY * sumZ) +
                     sumX * (sumYZ * sumY - sumYY * sumZ)) / determinant;

    double slopeY = (sumXX * (sumYZ * count - sumY * sumZ) -
                     sumXZ * (sumXY * count - sumY * sumX) +
                     sumX * (sumXY * sumZ - sumYZ * sumX)) / determinant;

    double offset = (sumXX * (sumYY * sumZ - sumYZ * sumY) -
                     sumXY * (sumXY * sumZ - sumYZ * sumX) +
                     sumXZ * (sumXY * sumY - sumYY * sumX)) / determinant;

    Vector3 normal = Vector3(-slopeX, -slopeY, 1).getNormalised();

    result = orientedPlane(normal, Vector3(0, 0, offset), interior);

    return true;
}

double planeHeightAt(const Plane3& plane, const Vector2& point)
{
    const Vector3& normal = plane.normal();

    if (std::fabs(normal.z()) < LINE_EPSILON)
    {
        return 0;
    }

    return (plane.dist() - normal.x() * point.x() - normal.y() * point.y()) / normal.z();
}

class HeightCache
{
public:
    explicit HeightCache(const std::vector<std::vector<Vector3>>& centrelines)
        : _centrelines(centrelines)
    {}

    double height(const Vector2& point)
    {
        auto key = std::make_pair(static_cast<long long>(std::llround(point.x() / POSITION_EPSILON)),
                                  static_cast<long long>(std::llround(point.y() / POSITION_EPSILON)));

        auto existing = _heights.find(key);

        if (existing != _heights.end())
        {
            return existing->second;
        }

        double value = anchorAt(_centrelines, point).position.z();
        _heights.emplace(key, value);

        return value;
    }

    std::vector<Vector3> raise(const Ring& ring, double offset)
    {
        std::vector<Vector3> points;

        for (const Vector2& point : ring)
        {
            points.push_back(lift(point, height(point) + offset));
        }

        return points;
    }

    bool coplanar(const Ring& ring)
    {
        std::vector<Vector3> points = raise(ring, 0);

        Plane3 plane;

        if (!ringPlane(points, Vector3(0, 0, -1000000.0), plane))
        {
            return false;
        }

        for (const Vector3& point : points)
        {
            if (std::fabs(plane.normal().dot(point) - plane.dist()) > COPLANAR_EPSILON)
            {
                return false;
            }
        }

        return true;
    }

private:
    const std::vector<std::vector<Vector3>>& _centrelines;
    std::map<std::pair<long long, long long>, double> _heights;
};

struct Corridor
{
    std::vector<std::vector<Vector3>> centrelines;
    Footprint footprint;
};

struct SideFace
{
    Plane3 plane;
    std::string material;
    Vector2 normal = Vector2(1, 0);
    Vector2 anchor = Vector2(0, 0);
    bool curb = false;
};

typedef std::array<long long, 4> PlaneKey;

PlaneKey planeKey(const Plane3& plane)
{
    return PlaneKey{ static_cast<long long>(std::llround(plane.normal().x() / PLANE_QUANTUM)),
                     static_cast<long long>(std::llround(plane.normal().y() / PLANE_QUANTUM)),
                     static_cast<long long>(std::llround(plane.normal().z() / PLANE_QUANTUM)),
                     static_cast<long long>(std::llround(plane.dist() / PLANE_QUANTUM)) };
}

void addFacets(BrushSolid& solid, const SideFace& side, const Plane3& top, double radius,
               const std::string& material, const Vector3& interior)
{
    Vector3 normal(side.normal.x(), side.normal.y(), 0);
    Vector3 axis = Vector3(side.anchor.x(), side.anchor.y(), 0) - normal * radius;
    axis.z() = planeHeightAt(top, side.anchor) - radius;

    for (std::size_t facet = 1; facet <= CURB_FACETS; ++facet)
    {
        double angle = (static_cast<double>(facet) - 0.5) * math::PI * 0.5 /
                       static_cast<double>(CURB_FACETS);

        Vector3 direction = normal * std::cos(angle) + Vector3(0, 0, std::sin(angle));

        BrushFace face;
        face.plane = orientedPlane(direction, axis + direction * radius, interior);
        face.material = material;
        solid.faces.push_back(face);
    }
}

void appendPiece(RoadPlan& plan, const Ring& ring, HeightCache& heights,
                 const Corridor& corridor, const RoadParams& params, bool sidewalk)
{
    if (ring.size() < 3 || std::fabs(polygon::ringArea(ring)) < MIN_AREA)
    {
        return;
    }

    double rise = sidewalk ? params.sidewalkHeight : 0;

    std::vector<Vector3> topRing = heights.raise(ring, rise);

    double baseHeight = topRing.front().z();

    for (const Vector3& point : topRing)
    {
        baseHeight = std::min(baseHeight, point.z());
    }

    baseHeight -= rise + BASE_DEPTH;

    Vector3 centre = centroidOf(topRing);
    Vector3 interior = Vector3(centre.x(), centre.y(), (centre.z() + baseHeight) * 0.5);

    Plane3 top;

    if (!ringPlane(topRing, interior, top))
    {
        return;
    }

    Plane3 bottom(Vector3(0, 0, -1), -baseHeight);

    BrushSolid solid;

    BrushFace topFace;
    topFace.plane = top;
    topFace.material = sidewalk ? params.sidewalkMaterial : params.roadMaterial;
    solid.faces.push_back(topFace);

    BrushFace bottomFace;
    bottomFace.plane = bottom;
    bottomFace.material = params.hiddenMaterial;
    solid.faces.push_back(bottomFace);

    std::map<PlaneKey, SideFace> sides;

    for (std::size_t index = 0; index < ring.size(); ++index)
    {
        const Vector2& from = ring[index];
        const Vector2& to = ring[(index + 1) % ring.size()];

        Vector2 along = to - from;

        if (along.getLength() < LINE_EPSILON)
        {
            continue;
        }

        Vector2 outward = Vector2(along.y(), -along.x()) / along.getLength();
        Vector2 middle = (from + to) * 0.5;

        SideFace side;
        side.normal = outward;
        side.anchor = middle;
        side.curb =
            sidewalk && onBoundary(corridor.footprint.carriage, middle, BOUNDARY_EPSILON);
        side.material = sidewalk ? params.curbMaterial : params.roadMaterial;
        side.plane = orientedPlane(Vector3(outward.x(), outward.y(), 0), lift(from, interior.z()),
                                   interior);

        PlaneKey key = planeKey(side.plane);
        auto existing = sides.find(key);

        if (existing == sides.end())
        {
            sides.emplace(key, side);
            continue;
        }

        if (side.curb)
        {
            existing->second.curb = true;
            existing->second.anchor = side.anchor;
        }
    }

    double radius = curbFilletRadius(params);

    for (const auto& entry : sides)
    {
        BrushFace face;
        face.plane = entry.second.plane;
        face.material = entry.second.material;
        solid.faces.push_back(face);

        if (entry.second.curb && params.curbStyle == CURB_ROUND && radius > 0)
        {
            addFacets(solid, entry.second, top, radius, params.curbMaterial, interior);
        }
    }

    if (solid.faces.size() < 4)
    {
        return;
    }

    plan.brushes.push_back(solid);
}

void appendRings(RoadPlan& plan, const std::vector<Ring>& rings, const Corridor& corridor,
                 const RoadParams& params, bool sidewalk)
{
    std::vector<Ring> triangles = polygon::triangulate(rings);

    if (triangles.empty())
    {
        return;
    }

    HeightCache heights(corridor.centrelines);

    std::vector<Ring> pieces = polygon::mergeConvex(
        triangles, [&heights](const Ring& ring) { return heights.coplanar(ring); });

    for (const Ring& piece : pieces)
    {
        appendPiece(plan, piece, heights, corridor, params, sidewalk);
    }
}

TextureFrame buildFrame(const std::vector<std::vector<Vector3>>& centrelines, double texScale)
{
    const std::vector<Vector3>* longest = &centrelines.front();
    double best = -1;

    for (const std::vector<Vector3>& line : centrelines)
    {
        double length = 0;

        for (std::size_t index = 1; index < line.size(); ++index)
        {
            length += (flatten(line[index]) - flatten(line[index - 1])).getLength();
        }

        if (length > best)
        {
            best = length;
            longest = &line;
        }
    }

    TextureFrame frame;
    frame.scale = texScale;
    frame.origin = longest->front();

    Vector2 chord = flatten(longest->back()) - flatten(longest->front());

    if (chord.getLength() > POSITION_EPSILON)
    {
        chord = chord / chord.getLength();
        frame.uAxis = Vector3(chord.x(), chord.y(), 0);
    }

    return frame;
}

} // anonymous namespace

bool faceProjection(const Plane3& plane, const TextureFrame& frame, FaceProjection& result)
{
    if (frame.scale <= 0)
    {
        return false;
    }

    Plane3 unit = plane.getNormalised();
    const Vector3& normal = unit.normal();
    double step = 1.0 / frame.scale;

    if (std::fabs(normal.z()) >= std::max(std::fabs(normal.x()), std::fabs(normal.y())))
    {
        Vector2 base = flatten(frame.origin);
        Vector2 alongU = flatten(frame.uAxis) * step;
        Vector2 alongV = flatten(frame.uAxis.cross(Vector3(0, 0, 1))) * step;

        result.points[0] = lift(base, planeHeightAt(plane, base));
        result.points[1] = lift(base + alongU, planeHeightAt(plane, base + alongU));
        result.points[2] = lift(base + alongV, planeHeightAt(plane, base + alongV));

        result.uvs[0] = Vector2(0, 0);
        result.uvs[1] = Vector2(1, 0);
        result.uvs[2] = Vector2(0, 1);

        return true;
    }

    Vector3 uAxis = Vector3(0, 0, 1).cross(normal).getNormalised();
    Vector3 down = -normal.cross(uAxis);
    Vector3 base = frame.origin - normal * (normal.dot(frame.origin) - unit.dist());

    result.points[0] = base;
    result.points[1] = base + uAxis * step;
    result.points[2] = base + down * step;

    result.uvs[0] = Vector2((base - frame.origin).dot(uAxis) * frame.scale,
                            (frame.origin.z() - base.z()) * frame.scale);
    result.uvs[1] = result.uvs[0] + Vector2(1, 0);
    result.uvs[2] = result.uvs[0] + Vector2(0, -down.z());

    return true;
}

double roadHalfWidth(const RoadParams& params)
{
    int lanes = params.lanes < 1 ? 1 : params.lanes;

    return lanes * params.laneWidth * 0.5;
}

RoadPlan buildPlan(const std::vector<std::vector<Vector3>>& curves, const RoadParams& params)
{
    RoadPlan plan;

    Corridor corridor;

    for (const std::vector<Vector3>& curve : curves)
    {
        std::vector<Vector3> sampled = sampleCurve(curve, params.subdivisions);

        if (sampled.size() < 2)
        {
            continue;
        }

        corridor.centrelines.push_back(sampled);
    }

    if (corridor.centrelines.empty() || roadHalfWidth(params) <= 0)
    {
        return plan;
    }

    plan.frame = buildFrame(corridor.centrelines, params.texScale);

    double sidewalkWidth = params.sidewalk ? params.sidewalkWidth : 0;
    bool rounded = params.cornerStyle == CORNER_ROUND;

    corridor.footprint = buildFootprint(corridor.centrelines, roadHalfWidth(params), sidewalkWidth,
                                        params.cornerRadius, rounded);

    appendRings(plan, corridor.footprint.carriage, corridor, params, false);
    appendRings(plan, corridor.footprint.band, corridor, params, true);

    return plan;
}

} // namespace road
