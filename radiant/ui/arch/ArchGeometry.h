#pragma once

#include "ibrush.h"
#include "iselection.h"
#include "scenelib.h"
#include "math/Plane3.h"
#include "math/Matrix3.h"
#include "math/Vector3.h"
#include "math/pi.h"

#include "math/FloatTools.h"

#include <cmath>
#include <vector>

namespace arch
{

const double DEG2RAD = math::PI / 180.0;

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

inline void generateArch(
    const Vector3& origin, int segments,
    double innerRadius, double wallThickness, double depth,
    double arcDegrees, double startAngle,
    double gridSize, const std::string& material, const scene::INodePtr& parent)
{
    double outerRadius = innerRadius + wallThickness;
    double anglePerSeg = arcDegrees / segments;
    double halfDepth = depth / 2.0;

    double yMin = float_snapped(origin.y() - halfDepth, gridSize);
    double yMax = float_snapped(origin.y() + halfDepth, gridSize);

    for (int i = 0; i < segments; ++i)
    {
        double a0 = (startAngle + i * anglePerSeg) * DEG2RAD;
        double a1 = (startAngle + (i + 1) * anglePerSeg) * DEG2RAD;

        // Side A: radial plane at a0
        double n0x = std::sin(a0);
        double n0z = -std::cos(a0);
        double p0x = origin.x() + std::cos(a0) * innerRadius;
        double p0z = origin.z() + std::sin(a0) * innerRadius;
        double distA = n0x * p0x + n0z * p0z;

        // Side B: radial plane at a1
        double n1x = -std::sin(a1);
        double n1z = std::cos(a1);
        double p1x = origin.x() + std::cos(a1) * innerRadius;
        double p1z = origin.z() + std::sin(a1) * innerRadius;
        double distB = n1x * p1x + n1z * p1z;

        double aMid = (a0 + a1) / 2.0;

        // Inner wall: chord plane passing through arc at a0
        double niX = -std::cos(aMid);
        double niZ = -std::sin(aMid);
        double distInner = niX * p0x + niZ * p0z;

        // Outer wall: chord plane passing through arc at a0
        double noX = std::cos(aMid);
        double noZ = std::sin(aMid);
        double poX = origin.x() + std::cos(a0) * outerRadius;
        double poZ = origin.z() + std::sin(a0) * outerRadius;
        double distOuter = noX * poX + noZ * poZ;

        std::vector<Plane3> faces;
        faces.push_back(Plane3(0, 1, 0, yMax));
        faces.push_back(Plane3(0, -1, 0, -yMin));
        faces.push_back(Plane3(n0x, 0, n0z, distA));
        faces.push_back(Plane3(n1x, 0, n1z, distB));
        faces.push_back(Plane3(niX, 0, niZ, distInner));
        faces.push_back(Plane3(noX, 0, noZ, distOuter));

        auto node = createWedgeBrush(faces, material, parent);
        Node_setSelected(node, true);
    }
}

struct BridgeEndpoints
{
    Vector3 a;
    Vector3 b;
    double faceDepth = 0;
    double facePerpCenter = 0;
    double faceSpanWidth = 0;
    double faceZExtent = 0;
    bool hasFaceDimensions = false;
    bool valid = false;
};

inline BridgeEndpoints detectBridgeEndpoints()
{
    BridgeEndpoints result;
    auto& sel = GlobalSelectionSystem();

    std::size_t faceCount = sel.getSelectedFaceCount();
    if (faceCount >= 2)
    {
        struct FaceData
        {
            Vector3 center;
            std::vector<Vector3> vertices;
        };

        std::vector<FaceData> allFaces;
        sel.foreachFace([&](IFace& face) {
            FaceData fd;
            fd.center = Vector3(0, 0, 0);
            const IWinding& winding = face.getWinding();
            for (std::size_t i = 0; i < winding.size(); ++i)
            {
                fd.vertices.push_back(winding[i].vertex);
                fd.center += winding[i].vertex;
            }
            if (winding.size() > 0)
                fd.center /= static_cast<double>(winding.size());
            allFaces.push_back(fd);
        });

        if (allFaces.size() >= faceCount && faceCount >= 2)
        {
            auto& fA = allFaces[allFaces.size() - faceCount];
            auto& fB = allFaces[allFaces.size() - 1];
            result.a = fA.center;
            result.b = fB.center;
            result.valid = true;

            if (!fA.vertices.empty())
            {
                double dx = result.b.x() - result.a.x();
                double dy = result.b.y() - result.a.y();
                double dist = std::sqrt(dx * dx + dy * dy);
                if (dist < 1.0) dist = 1.0;

                double spanX = dx / dist;
                double spanY = dy / dist;
                double perpX = -spanY;
                double perpY = spanX;

                double minPerp = fA.vertices[0].x() * perpX + fA.vertices[0].y() * perpY;
                double maxPerp = minPerp;
                double minSpan = fA.vertices[0].x() * spanX + fA.vertices[0].y() * spanY;
                double maxSpan = minSpan;
                double minZ = fA.vertices[0].z();
                double maxZ = minZ;
                for (std::size_t i = 1; i < fA.vertices.size(); ++i)
                {
                    double proj = fA.vertices[i].x() * perpX + fA.vertices[i].y() * perpY;
                    minPerp = std::min(minPerp, proj);
                    maxPerp = std::max(maxPerp, proj);
                    double sp = fA.vertices[i].x() * spanX + fA.vertices[i].y() * spanY;
                    minSpan = std::min(minSpan, sp);
                    maxSpan = std::max(maxSpan, sp);
                    minZ = std::min(minZ, fA.vertices[i].z());
                    maxZ = std::max(maxZ, fA.vertices[i].z());
                }

                double d = maxPerp - minPerp;
                if (d > 0.1)
                {
                    result.faceDepth = d;
                    result.facePerpCenter = (minPerp + maxPerp) / 2.0;
                    result.hasFaceDimensions = true;
                }
                result.faceSpanWidth = maxSpan - minSpan;
                result.faceZExtent = maxZ - minZ;
            }

            return result;
        }
    }

    if (sel.countSelected() >= 2)
    {
        std::vector<Vector3> origins;
        sel.foreachSelected([&](const scene::INodePtr& node) {
            AABB bounds = node->worldAABB();
            if (bounds.isValid())
                origins.push_back(bounds.getOrigin());
        });

        if (origins.size() >= 2)
        {
            result.a = origins[origins.size() - 2];
            result.b = origins[origins.size() - 1];
            result.valid = true;
            return result;
        }
    }

    return result;
}

inline void generateBridgeArch(
    const BridgeEndpoints& endpoints,
    int segments, double wallThickness, double depth,
    double arcDegrees,
    const std::string& material, const scene::INodePtr& parent)
{
    const Vector3& pointA = endpoints.a;
    const Vector3& pointB = endpoints.b;
    Vector3 midpoint = (pointA + pointB) * 0.5;

    double dx = pointB.x() - pointA.x();
    double dy = pointB.y() - pointA.y();
    double horizontalDist = std::sqrt(dx * dx + dy * dy);

    if (horizontalDist < 1.0)
        horizontalDist = 1.0;

    double halfArc = (arcDegrees / 2.0) * DEG2RAD;
    double sinHalf = std::sin(halfArc);
    double cosHalf = std::cos(halfArc);

    double innerRadius;
    double outerRadius;
    if (endpoints.faceSpanWidth > 0.1)
    {
        innerRadius = (horizontalDist - endpoints.faceSpanWidth) / (2.0 * sinHalf);
        outerRadius = (horizontalDist + endpoints.faceSpanWidth) / (2.0 * sinHalf);
    }
    else if (endpoints.faceZExtent > 0.1 && cosHalf > 1e-6)
    {
        double midR = horizontalDist / (2.0 * sinHalf);
        double halfWall = endpoints.faceZExtent / (2.0 * cosHalf);
        innerRadius = midR - halfWall;
        outerRadius = midR + halfWall;
    }
    else
    {
        innerRadius = (horizontalDist / 2.0) / sinHalf;
        outerRadius = innerRadius + wallThickness;
    }

    double anglePerSeg = arcDegrees / segments;
    double halfDepth = depth / 2.0;

    double dirAngle = std::atan2(dy, dx);
    double startAngle = 90.0 - arcDegrees / 2.0;

    double faceZ = (pointA.z() + pointB.z()) * 0.5;
    double midRadius = (innerRadius + outerRadius) / 2.0;
    double cz = faceZ - cosHalf * midRadius;
    double cx = midpoint.x();
    double cy = midpoint.y();

    double cosDir = std::cos(dirAngle);
    double sinDir = std::sin(dirAngle);

    double perpX = -sinDir;
    double perpY = cosDir;

    double depthCenter = endpoints.hasFaceDimensions
        ? endpoints.facePerpCenter
        : (cx * perpX + cy * perpY);

    for (int i = 0; i < segments; ++i)
    {
        double a0 = (startAngle + i * anglePerSeg) * DEG2RAD;
        double a1 = (startAngle + (i + 1) * anglePerSeg) * DEG2RAD;

        double aMid = (a0 + a1) / 2.0;

        auto toWorld = [&](double localX, double localZ) -> Vector3 {
            return Vector3(
                cx + localX * cosDir,
                cy + localX * sinDir,
                cz + localZ);
        };

        Vector3 innerA0 = toWorld(std::cos(a0) * innerRadius, std::sin(a0) * innerRadius);
        Vector3 innerA1 = toWorld(std::cos(a1) * innerRadius, std::sin(a1) * innerRadius);

        // Side A: radial plane at a0
        double ln0x = std::sin(a0);
        double ln0z = -std::cos(a0);
        double wn0x = ln0x * cosDir;
        double wn0y = ln0x * sinDir;
        double wn0z = ln0z;
        double distA = wn0x * innerA0.x() + wn0y * innerA0.y() + wn0z * innerA0.z();

        // Side B: radial plane at a1
        double ln1x = -std::sin(a1);
        double ln1z = std::cos(a1);
        double wn1x = ln1x * cosDir;
        double wn1y = ln1x * sinDir;
        double wn1z = ln1z;
        double distB = wn1x * innerA1.x() + wn1y * innerA1.y() + wn1z * innerA1.z();

        // Inner wall: chord plane passing through arc at a0
        double lniX = -std::cos(aMid);
        double lniZ = -std::sin(aMid);
        double wniX = lniX * cosDir;
        double wniY = lniX * sinDir;
        double wniZ = lniZ;
        double distInner = wniX * innerA0.x() + wniY * innerA0.y() + wniZ * innerA0.z();

        // Outer wall: chord plane passing through arc at a0
        double wnoX = -wniX;
        double wnoY = -wniY;
        double wnoZ = -wniZ;
        Vector3 outerA0 = toWorld(std::cos(a0) * outerRadius, std::sin(a0) * outerRadius);
        double distOuter = wnoX * outerA0.x() + wnoY * outerA0.y() + wnoZ * outerA0.z();

        double distFront = depthCenter + halfDepth;
        double distBack = -(depthCenter - halfDepth);

        std::vector<Plane3> faces;
        faces.push_back(Plane3(perpX, perpY, 0, distFront));
        faces.push_back(Plane3(-perpX, -perpY, 0, distBack));
        faces.push_back(Plane3(wn0x, wn0y, wn0z, distA));
        faces.push_back(Plane3(wn1x, wn1y, wn1z, distB));
        faces.push_back(Plane3(wniX, wniY, wniZ, distInner));
        faces.push_back(Plane3(wnoX, wnoY, wnoZ, distOuter));

        auto node = createWedgeBrush(faces, material, parent);
        Node_setSelected(node, true);
    }
}

} // namespace arch
