#include "WallGeometry.h"

#include "math/Plane3.h"
#include "math/Matrix3.h"
#include "math/pi.h"
#include "ui/building/BuildingGeometry.h"

#include <cmath>

namespace ui
{

namespace wallgeometry
{

namespace
{

constexpr double Epsilon = 1e-6;

Matrix3 defaultProjection()
{
    double texScale = building::getTextureScale();
    Matrix3 proj = Matrix3::getIdentity();
    proj.xx() = texScale;
    proj.yy() = texScale;
    return proj;
}

double cross2(const Vector2& a, const Vector2& b)
{
    return a.x() * b.y() - a.y() * b.x();
}

bool pointInTriangle(const Vector2& p, const Vector2& a, const Vector2& b, const Vector2& c)
{
    double d1 = cross2(b - a, p - a);
    double d2 = cross2(c - b, p - b);
    double d3 = cross2(a - c, p - c);

    bool hasNeg = (d1 < -Epsilon) || (d2 < -Epsilon) || (d3 < -Epsilon);
    bool hasPos = (d1 > Epsilon) || (d2 > Epsilon) || (d3 > Epsilon);

    return !(hasNeg && hasPos);
}

std::vector<std::vector<Vector2>> triangulate(const std::vector<Vector2>& polygon)
{
    std::vector<std::vector<Vector2>> triangles;
    std::vector<Vector2> remaining = polygon;

    std::size_t guard = remaining.size() * remaining.size() + 16;

    while (remaining.size() > 3 && guard-- > 0)
    {
        bool clipped = false;

        for (std::size_t i = 0; i < remaining.size(); ++i)
        {
            std::size_t prev = (i + remaining.size() - 1) % remaining.size();
            std::size_t next = (i + 1) % remaining.size();

            const Vector2& a = remaining[prev];
            const Vector2& b = remaining[i];
            const Vector2& c = remaining[next];

            if (cross2(b - a, c - b) <= Epsilon)
            {
                continue;
            }

            bool containsOther = false;

            for (std::size_t j = 0; j < remaining.size(); ++j)
            {
                if (j == prev || j == i || j == next) continue;

                if (pointInTriangle(remaining[j], a, b, c))
                {
                    containsOther = true;
                    break;
                }
            }

            if (containsOther)
            {
                continue;
            }

            triangles.push_back({ a, b, c });
            remaining.erase(remaining.begin() + i);
            clipped = true;
            break;
        }

        if (!clipped)
        {
            return {};
        }
    }

    if (remaining.size() == 3)
    {
        triangles.push_back(remaining);
    }

    return triangles;
}

bool pointsEqual(const Vector2& a, const Vector2& b)
{
    return std::abs(a.x() - b.x()) < 0.01 && std::abs(a.y() - b.y()) < 0.01;
}

bool tryMerge(const std::vector<Vector2>& p, const std::vector<Vector2>& q,
    std::vector<Vector2>& result)
{
    for (std::size_t i = 0; i < p.size(); ++i)
    {
        const Vector2& a = p[i];
        const Vector2& b = p[(i + 1) % p.size()];

        for (std::size_t j = 0; j < q.size(); ++j)
        {
            if (!pointsEqual(q[j], b) || !pointsEqual(q[(j + 1) % q.size()], a))
            {
                continue;
            }

            std::vector<Vector2> merged;

            for (std::size_t k = 0; k <= i; ++k)
            {
                merged.push_back(p[k]);
            }

            for (std::size_t k = 2; k < q.size(); ++k)
            {
                merged.push_back(q[(j + k) % q.size()]);
            }

            for (std::size_t k = i + 1; k < p.size(); ++k)
            {
                merged.push_back(p[k]);
            }

            if (!isConvex(merged))
            {
                return false;
            }

            result = std::move(merged);
            return true;
        }
    }

    return false;
}

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
    double baseZ, double height, double thickness, const std::string& material,
    const std::optional<WallJointCap>& capA, const std::optional<WallJointCap>& capB)
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

    if (capB)
    {
        brush.addFace(Plane3(capB->normal.x(), capB->normal.y(), 0, capB->dist), proj, material);
    }
    else
    {
        brush.addFace(Plane3(dir.x(), dir.y(), 0, dir.dot(b)), proj, material);
    }

    if (capA)
    {
        brush.addFace(Plane3(capA->normal.x(), capA->normal.y(), 0, capA->dist), proj, material);
    }
    else
    {
        brush.addFace(Plane3(-dir.x(), -dir.y(), 0, -dir.dot(a)), proj, material);
    }

    brush.evaluateBRep();
}

std::optional<WallJointCaps> computeButtJointCaps(const Vector2& corner,
    const Vector2& segmentAway, double segmentThickness,
    const Vector2& otherAway, double otherThickness)
{
    if (std::abs(segmentAway.x() * otherAway.y() - segmentAway.y() * otherAway.x()) < 0.001)
    {
        return {};
    }

    Vector2 perpOther(-otherAway.y(), otherAway.x());
    Vector2 sideN = perpOther.dot(segmentAway) > 0 ? perpOther : Vector2(-perpOther.x(), -perpOther.y());
    double nearDist = sideN.dot(corner) + otherThickness * 0.5;

    Vector2 perpSegment(-segmentAway.y(), segmentAway.x());
    Vector2 farN = perpSegment.dot(otherAway) < 0 ? perpSegment : Vector2(-perpSegment.x(), -perpSegment.y());
    double farDist = farN.dot(corner) + segmentThickness * 0.5;

    double det = sideN.x() * farN.y() - sideN.y() * farN.x();

    if (std::abs(det) < 1e-6)
    {
        return {};
    }

    Vector2 outerPoint((nearDist * farN.y() - farDist * sideN.y()) / det,
        (sideN.x() * farDist - farN.x() * nearDist) / det);

    WallJointCaps caps;
    caps.segmentCap.normal = Vector2(-sideN.x(), -sideN.y());
    caps.segmentCap.dist = -nearDist;
    caps.otherCap.normal = Vector2(-otherAway.x(), -otherAway.y());
    caps.otherCap.dist = caps.otherCap.normal.dot(outerPoint);

    return caps;
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

void buildShedRoofFaces(IBrush& brush, const Vector2& mins, const Vector2& maxs,
    double baseZ, double rise, const std::string& material)
{
    Matrix3 proj = defaultProjection();

    brush.clear();
    brush.addFace(Plane3(1, 0, 0, maxs.x()), proj, material);
    brush.addFace(Plane3(-1, 0, 0, -mins.x()), proj, material);
    brush.addFace(Plane3(0, 1, 0, maxs.y()), proj, material);
    brush.addFace(Plane3(0, -1, 0, -mins.y()), proj, material);
    brush.addFace(Plane3(0, 0, -1, -baseZ), proj, material);

    double spanX = maxs.x() - mins.x();
    double spanY = maxs.y() - mins.y();

    if (spanY <= spanX)
    {
        double length = std::sqrt(rise * rise + spanY * spanY);
        double ny = -rise / length;
        double nz = spanY / length;
        brush.addFace(Plane3(0, ny, nz, ny * maxs.y() + nz * (baseZ + rise)), proj, material);
    }
    else
    {
        double length = std::sqrt(rise * rise + spanX * spanX);
        double nx = -rise / length;
        double nz = spanX / length;
        brush.addFace(Plane3(nx, 0, nz, nx * maxs.x() + nz * (baseZ + rise)), proj, material);
    }

    brush.evaluateBRep();
}

void buildGabledRoofWedgeFaces(IBrush& brush, const Vector2& mins, const Vector2& maxs,
    double baseZ, double rise, bool firstHalf, const std::string& material)
{
    Matrix3 proj = defaultProjection();

    double spanX = maxs.x() - mins.x();
    double spanY = maxs.y() - mins.y();

    brush.clear();
    brush.addFace(Plane3(0, 0, -1, -baseZ), proj, material);

    if (spanY <= spanX)
    {
        double mid = (mins.y() + maxs.y()) * 0.5;
        double half = spanY * 0.5;
        double length = std::sqrt(rise * rise + half * half);

        brush.addFace(Plane3(1, 0, 0, maxs.x()), proj, material);
        brush.addFace(Plane3(-1, 0, 0, -mins.x()), proj, material);

        if (firstHalf)
        {
            brush.addFace(Plane3(0, 1, 0, maxs.y()), proj, material);
            brush.addFace(Plane3(0, -1, 0, -mid), proj, material);

            double ny = rise / length;
            double nz = half / length;
            brush.addFace(Plane3(0, ny, nz, ny * maxs.y() + nz * baseZ), proj, material);
        }
        else
        {
            brush.addFace(Plane3(0, 1, 0, mid), proj, material);
            brush.addFace(Plane3(0, -1, 0, -mins.y()), proj, material);

            double ny = -rise / length;
            double nz = half / length;
            brush.addFace(Plane3(0, ny, nz, ny * mins.y() + nz * baseZ), proj, material);
        }
    }
    else
    {
        double mid = (mins.x() + maxs.x()) * 0.5;
        double half = spanX * 0.5;
        double length = std::sqrt(rise * rise + half * half);

        brush.addFace(Plane3(0, 1, 0, maxs.y()), proj, material);
        brush.addFace(Plane3(0, -1, 0, -mins.y()), proj, material);

        if (firstHalf)
        {
            brush.addFace(Plane3(1, 0, 0, maxs.x()), proj, material);
            brush.addFace(Plane3(-1, 0, 0, -mid), proj, material);

            double nx = rise / length;
            double nz = half / length;
            brush.addFace(Plane3(nx, 0, nz, nx * maxs.x() + nz * baseZ), proj, material);
        }
        else
        {
            brush.addFace(Plane3(1, 0, 0, mid), proj, material);
            brush.addFace(Plane3(-1, 0, 0, -mins.x()), proj, material);

            double nx = -rise / length;
            double nz = half / length;
            brush.addFace(Plane3(nx, 0, nz, nx * mins.x() + nz * baseZ), proj, material);
        }
    }

    brush.evaluateBRep();
}

std::vector<Vector2> simplifyCollinear(const std::vector<Vector2>& polygon)
{
    std::vector<Vector2> result;

    for (std::size_t i = 0; i < polygon.size(); ++i)
    {
        const Vector2& prev = polygon[(i + polygon.size() - 1) % polygon.size()];
        const Vector2& curr = polygon[i];
        const Vector2& next = polygon[(i + 1) % polygon.size()];

        if ((curr - prev).getLengthSquared() < Epsilon)
        {
            continue;
        }

        if (std::abs(cross2(curr - prev, next - curr)) < Epsilon)
        {
            continue;
        }

        result.push_back(curr);
    }

    return result;
}

double signedArea(const std::vector<Vector2>& polygon)
{
    double area = 0;

    for (std::size_t i = 0; i < polygon.size(); ++i)
    {
        const Vector2& p = polygon[i];
        const Vector2& q = polygon[(i + 1) % polygon.size()];
        area += cross2(p, q);
    }

    return area * 0.5;
}

bool isConvex(const std::vector<Vector2>& polygon)
{
    if (polygon.size() < 3)
    {
        return false;
    }

    for (std::size_t i = 0; i < polygon.size(); ++i)
    {
        const Vector2& a = polygon[(i + polygon.size() - 1) % polygon.size()];
        const Vector2& b = polygon[i];
        const Vector2& c = polygon[(i + 1) % polygon.size()];

        if (cross2(b - a, c - b) < -Epsilon)
        {
            return false;
        }
    }

    return true;
}

bool isAxisAlignedRectangle(const std::vector<Vector2>& polygon)
{
    if (polygon.size() != 4)
    {
        return false;
    }

    for (std::size_t i = 0; i < 4; ++i)
    {
        Vector2 edge = polygon[(i + 1) % 4] - polygon[i];

        if (std::abs(edge.x()) > Epsilon && std::abs(edge.y()) > Epsilon)
        {
            return false;
        }
    }

    return true;
}

std::vector<std::vector<Vector2>> decomposeIntoConvex(const std::vector<Vector2>& polygon)
{
    auto pieces = triangulate(polygon);

    if (pieces.empty())
    {
        return {};
    }

    bool merged = true;

    while (merged)
    {
        merged = false;

        for (std::size_t i = 0; i < pieces.size() && !merged; ++i)
        {
            for (std::size_t j = i + 1; j < pieces.size() && !merged; ++j)
            {
                std::vector<Vector2> result;

                if (tryMerge(pieces[i], pieces[j], result))
                {
                    pieces[i] = std::move(result);
                    pieces.erase(pieces.begin() + j);
                    merged = true;
                }
            }
        }
    }

    for (auto& piece : pieces)
    {
        piece = simplifyCollinear(piece);
    }

    return pieces;
}

}

}
