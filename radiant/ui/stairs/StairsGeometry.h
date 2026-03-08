#pragma once

#include "ibrush.h"
#include "scenelib.h"
#include "math/Plane3.h"
#include "math/Matrix3.h"
#include "math/Vector3.h"

#include <cmath>
#include <vector>
#include <algorithm>

namespace stairs
{

constexpr double PI = 3.14159265358979323846;
constexpr double DEG2RAD = PI / 180.0;

struct StairDir
{
    double dx, dy; // forward direction
    double px, py; // perpendicular (left when facing forward)
};

inline StairDir dirFromDegrees(double deg)
{
    double rad = deg * DEG2RAD;
    StairDir d;
    d.dx = std::cos(rad);
    d.dy = std::sin(rad);
    d.px = -d.dy;
    d.py = d.dx;
    return d;
}

inline scene::INodePtr createBoxBrush(
    const Vector3& mins, const Vector3& maxs,
    const std::string& material, const scene::INodePtr& parent)
{
    auto brushNode = GlobalBrushCreator().createBrush();
    parent->addChildNode(brushNode);

    auto& brush = *Node_getIBrush(brushNode);

    double texScale = 0.0078125;
    Matrix3 proj = Matrix3::getIdentity();
    proj.xx() = texScale;
    proj.yy() = texScale;

    brush.addFace(Plane3( 1, 0, 0,  maxs.x()), proj, material);
    brush.addFace(Plane3(-1, 0, 0, -mins.x()), proj, material);
    brush.addFace(Plane3( 0, 1, 0,  maxs.y()), proj, material);
    brush.addFace(Plane3( 0,-1, 0, -mins.y()), proj, material);
    brush.addFace(Plane3( 0, 0, 1,  maxs.z()), proj, material);
    brush.addFace(Plane3( 0, 0,-1, -mins.z()), proj, material);

    brush.evaluateBRep();
    return brushNode;
}

inline scene::INodePtr createWedgeBrush(
    const std::vector<Plane3>& faces,
    const std::string& material, const scene::INodePtr& parent)
{
    auto brushNode = GlobalBrushCreator().createBrush();
    parent->addChildNode(brushNode);

    auto& brush = *Node_getIBrush(brushNode);

    double texScale = 0.0078125;
    Matrix3 proj = Matrix3::getIdentity();
    proj.xx() = texScale;
    proj.yy() = texScale;

    for (auto& plane : faces)
        brush.addFace(plane, proj, material);

    brush.evaluateBRep();
    return brushNode;
}

inline void generateStraightStairs(
    const Vector3& origin, int stepCount,
    double stepH, double stepD, double width,
    double dirDeg, bool solid, const std::string& material,
    const scene::INodePtr& parent)
{
    auto dir = dirFromDegrees(dirDeg);

    for (int i = 0; i < stepCount; ++i)
    {
        double depthStart = i * stepD;
        double depthEnd = (i + 1) * stepD;
        double zBot = solid ? origin.z() : origin.z() + i * stepH;
        double zTop = origin.z() + (i + 1) * stepH;

        double c0x = origin.x() + dir.dx * depthStart;
        double c0y = origin.y() + dir.dy * depthStart;
        double c1x = origin.x() + dir.dx * depthEnd;
        double c1y = origin.y() + dir.dy * depthEnd;
        double c2x = c0x + dir.px * width;
        double c2y = c0y + dir.py * width;
        double c3x = c1x + dir.px * width;
        double c3y = c1y + dir.py * width;

        Vector3 mins(
            std::min({c0x, c1x, c2x, c3x}),
            std::min({c0y, c1y, c2y, c3y}),
            zBot);
        Vector3 maxs(
            std::max({c0x, c1x, c2x, c3x}),
            std::max({c0y, c1y, c2y, c3y}),
            zTop);

        auto node = createBoxBrush(mins, maxs, material, parent);
        Node_setSelected(node, true);
    }
}

inline void generateLShapeStairs(
    const Vector3& origin, int stepCount,
    double stepH, double stepD, double width,
    double dirDeg, bool solid, int turnAt, int turnDir,
    const std::string& material, const scene::INodePtr& parent)
{
    double turnSign = (turnDir == 1) ? -1.0 : 1.0; // 0=left, 1=right
    auto dir = dirFromDegrees(dirDeg);

    // First run
    generateStraightStairs(origin, turnAt, stepH, stepD, width, dirDeg, solid, material, parent);

    // Landing
    double landX = origin.x() + dir.dx * turnAt * stepD;
    double landY = origin.y() + dir.dy * turnAt * stepD;
    double landZ = solid ? origin.z() : origin.z() + turnAt * stepH;
    double landZTop = origin.z() + (turnAt + 1) * stepH;

    double lx0 = landX, ly0 = landY;
    double lx1 = landX + dir.dx * width, ly1 = landY + dir.dy * width;
    double lx2 = landX + dir.px * width * turnSign;
    double ly2 = landY + dir.py * width * turnSign;
    double lx3 = lx1 + dir.px * width * turnSign;
    double ly3 = ly1 + dir.py * width * turnSign;

    Vector3 lMins(std::min({lx0, lx1, lx2, lx3}), std::min({ly0, ly1, ly2, ly3}), landZ);
    Vector3 lMaxs(std::max({lx0, lx1, lx2, lx3}), std::max({ly0, ly1, ly2, ly3}), landZTop);
    auto landNode = createBoxBrush(lMins, lMaxs, material, parent);
    Node_setSelected(landNode, true);

    // Second run: turned 90 degrees
    int run2Count = stepCount - turnAt - 1;
    if (run2Count > 0)
    {
        double newDirDeg = dirDeg + 90.0 * turnSign;
        double run2Ox = landX + dir.dx * width + dir.px * width * turnSign;
        double run2Oy = landY + dir.dy * width + dir.py * width * turnSign;
        double run2Oz = origin.z() + (turnAt + 1) * stepH;

        generateStraightStairs(
            Vector3(run2Ox, run2Oy, run2Oz), run2Count,
            stepH, stepD, width, newDirDeg, solid, material, parent);
    }
}

inline void generateUShapeStairs(
    const Vector3& origin, int stepCount,
    double stepH, double stepD, double width,
    double dirDeg, bool solid, int turnAt, int turnDir,
    double landingDepth, const std::string& material,
    const scene::INodePtr& parent)
{
    double turnSign = (turnDir == 1) ? -1.0 : 1.0;
    auto dir = dirFromDegrees(dirDeg);

    // First run
    generateStraightStairs(origin, turnAt, stepH, stepD, width, dirDeg, solid, material, parent);

    // Landing
    double landX = origin.x() + dir.dx * turnAt * stepD;
    double landY = origin.y() + dir.dy * turnAt * stepD;
    double landZ = solid ? origin.z() : origin.z() + turnAt * stepH;
    double landZTop = origin.z() + (turnAt + 1) * stepH;

    double lx0 = landX, ly0 = landY;
    double lx1 = landX + dir.dx * landingDepth, ly1 = landY + dir.dy * landingDepth;
    double lx2 = landX + dir.px * width * 2 * turnSign;
    double ly2 = landY + dir.py * width * 2 * turnSign;
    double lx3 = lx1 + dir.px * width * 2 * turnSign;
    double ly3 = ly1 + dir.py * width * 2 * turnSign;

    Vector3 lMins(std::min({lx0, lx1, lx2, lx3}), std::min({ly0, ly1, ly2, ly3}), landZ);
    Vector3 lMaxs(std::max({lx0, lx1, lx2, lx3}), std::max({ly0, ly1, ly2, ly3}), landZTop);
    auto landNode = createBoxBrush(lMins, lMaxs, material, parent);
    Node_setSelected(landNode, true);

    // Second run: 180 degrees, offset sideways
    int run2Count = stepCount - turnAt - 1;
    if (run2Count > 0)
    {
        double newDirDeg = dirDeg + 180.0;
        double run2Ox = landX + dir.px * 2 * width * turnSign;
        double run2Oy = landY + dir.py * 2 * width * turnSign;
        double run2Oz = origin.z() + (turnAt + 1) * stepH;

        generateStraightStairs(
            Vector3(run2Ox, run2Oy, run2Oz), run2Count,
            stepH, stepD, width, newDirDeg, solid, material, parent);
    }
}

inline void generateSpiralStairs(
    const Vector3& origin, int stepCount,
    double stepH, double dirDeg, bool solid,
    double innerR, double outerR, double totalAngle,
    const std::string& material, const scene::INodePtr& parent)
{
    double cx = origin.x();
    double cy = origin.y();
    double anglePerStep = totalAngle / stepCount;

    for (int i = 0; i < stepCount; ++i)
    {
        double a0 = (dirDeg + i * anglePerStep) * DEG2RAD;
        double a1 = (dirDeg + (i + 1) * anglePerStep) * DEG2RAD;
        double zBot = solid ? origin.z() : origin.z() + i * stepH;
        double zTop = origin.z() + (i + 1) * stepH;

        // Side A: radial plane at angle a0, outward normal points clockwise
        double n0x = std::sin(a0);
        double n0y = -std::cos(a0);
        double pa0x = cx + std::cos(a0) * innerR;
        double pa0y = cy + std::sin(a0) * innerR;
        double distA = n0x * pa0x + n0y * pa0y;

        // Side B: radial plane at angle a1, outward normal points counter-clockwise
        double n1x = -std::sin(a1);
        double n1y = std::cos(a1);
        double pa1x = cx + std::cos(a1) * innerR;
        double pa1y = cy + std::sin(a1) * innerR;
        double distB = n1x * pa1x + n1y * pa1y;

        // Inner wall: chord plane, normal points toward center
        double aMid = (a0 + a1) / 2.0;
        double niX = -std::cos(aMid);
        double niY = -std::sin(aMid);
        double piX = cx + std::cos(aMid) * innerR;
        double piY = cy + std::sin(aMid) * innerR;
        double distInner = niX * piX + niY * piY;

        // Outer wall: chord plane, normal points away from center
        double noX = std::cos(aMid);
        double noY = std::sin(aMid);
        double poX = cx + std::cos(aMid) * outerR;
        double poY = cy + std::sin(aMid) * outerR;
        double distOuter = noX * poX + noY * poY;

        std::vector<Plane3> faces;
        faces.push_back(Plane3(0, 0, 1, zTop));
        faces.push_back(Plane3(0, 0, -1, -zBot));
        faces.push_back(Plane3(n0x, n0y, 0, distA));
        faces.push_back(Plane3(n1x, n1y, 0, distB));
        faces.push_back(Plane3(niX, niY, 0, distInner));
        faces.push_back(Plane3(noX, noY, 0, distOuter));

        auto node = createWedgeBrush(faces, material, parent);
        Node_setSelected(node, true);
    }
}

} // namespace stairs
