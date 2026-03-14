#pragma once

#include "ibrush.h"
#include "scenelib.h"
#include "math/AABB.h"
#include "math/Plane3.h"
#include "math/Matrix3.h"
#include "math/Matrix4.h"
#include "math/Vector3.h"

#include "../cables/CableGeometry.h"

#include <cmath>
#include <vector>
#include <string>

namespace sweep
{

struct SweepParams
{
    int segments = 8;
};

struct SourceFaceData
{
    Vector3 points[3];
    Vector3 normal;
    std::string material;
    Matrix3 projection;
};

struct SourceBrushData
{
    std::vector<SourceFaceData> faces;
};

inline double sweepCoord(const Vector3& v, int axis)
{
    if (axis == 0) return v.x();
    if (axis == 1) return v.y();
    return v.z();
}

inline void crossCoords(const Vector3& v, const Vector3& origin,
    int sweepAxis, double& ca, double& cb)
{
    if (sweepAxis == 0)
    {
        ca = v.y() - origin.y();
        cb = v.z() - origin.z();
    }
    else if (sweepAxis == 1)
    {
        ca = v.x() - origin.x();
        cb = v.z() - origin.z();
    }
    else
    {
        ca = v.x() - origin.x();
        cb = v.y() - origin.y();
    }
}

inline void refCrossCoords(const Vector3& srcVertex, const Vector3& srcOrigin,
    const Vector3& faceNormal, int sweepAxis, double sweepRef, double& ca, double& cb)
{
    double na, nb, nc;
    if (sweepAxis == 0)
    {
        na = faceNormal.x(); nb = faceNormal.y(); nc = faceNormal.z();
    }
    else if (sweepAxis == 1)
    {
        na = faceNormal.y(); nb = faceNormal.x(); nc = faceNormal.z();
    }
    else
    {
        na = faceNormal.z(); nb = faceNormal.x(); nc = faceNormal.y();
    }

    double denom = 1.0 - na * na;
    if (std::abs(denom) > 1e-8)
    {
        double slide = (sweepRef - sweepCoord(srcVertex, sweepAxis)) / denom;
        double rawA, rawB;
        crossCoords(srcVertex, srcOrigin, sweepAxis, rawA, rawB);
        ca = rawA - na * nb * slide;
        cb = rawB - na * nc * slide;
    }
    else
    {
        crossCoords(srcVertex, srcOrigin, sweepAxis, ca, cb);
    }
}

inline Vector3 projectToFrame(const Vector3& srcVertex,
    const Vector3& srcOrigin, const Vector3& faceNormal,
    int sweepAxis, double sweepRef, const cables::Frame& frame)
{
    double ca, cb;
    refCrossCoords(srcVertex, srcOrigin, faceNormal, sweepAxis, sweepRef, ca, cb);
    return frame.position + frame.normal * ca + frame.binormal * cb;
}

inline Vector3 transformSweepVertex(const Vector3& srcVertex,
    const Vector3& srcOrigin, const Vector3& faceNormal,
    int sweepAxis, double sweepMid,
    const cables::Frame& startFrame, const cables::Frame& endFrame)
{
    if (sweepCoord(srcVertex, sweepAxis) < sweepMid)
        return projectToFrame(srcVertex, srcOrigin, faceNormal, sweepAxis, sweepMid, startFrame);
    else
        return projectToFrame(srcVertex, srcOrigin, faceNormal, sweepAxis, sweepMid, endFrame);
}

inline Vector3 approxTransformNormal(const Vector3& srcNormal,
    int sweepAxis, const cables::Frame& frame)
{
    double ct, ca, cb;
    if (sweepAxis == 0)
    {
        ct = srcNormal.x(); ca = srcNormal.y(); cb = srcNormal.z();
    }
    else if (sweepAxis == 1)
    {
        ca = srcNormal.x(); ct = srcNormal.y(); cb = srcNormal.z();
    }
    else
    {
        ca = srcNormal.x(); cb = srcNormal.y(); ct = srcNormal.z();
    }
    return frame.tangent * ct + frame.normal * ca + frame.binormal * cb;
}

inline std::vector<SourceBrushData> extractSourceBrushes(
    const std::vector<scene::INodePtr>& brushNodes, AABB& combinedBounds)
{
    std::vector<SourceBrushData> result;
    combinedBounds = AABB();

    for (auto& node : brushNodes)
    {
        auto* brush = Node_getIBrush(node);
        if (!brush) continue;

        brush->evaluateBRep();

        SourceBrushData sbd;
        for (std::size_t f = 0; f < brush->getNumFaces(); ++f)
        {
            auto& face = brush->getFace(f);
            const IWinding& winding = face.getWinding();
            if (winding.size() < 3) continue;

            SourceFaceData sfd;
            sfd.points[0] = winding[0].vertex;
            sfd.points[1] = winding[1].vertex;
            sfd.points[2] = winding[2].vertex;
            sfd.normal = face.getPlane3().normal();
            sfd.material = face.getShader();
            sfd.projection = face.getProjectionMatrix();
            sbd.faces.push_back(sfd);

            for (std::size_t v = 0; v < winding.size(); ++v)
                combinedBounds.includePoint(winding[v].vertex);
        }

        result.push_back(sbd);
    }

    return result;
}

inline int detectSweepAxis(
    const std::vector<scene::INodePtr>& brushNodes, const AABB& combinedBounds)
{
    for (int axis = 0; axis < 3; ++axis)
    {
        bool allMatch = true;
        double refLo = 0, refHi = 0;
        bool first = true;

        for (auto& node : brushNodes)
        {
            AABB nb = node->worldAABB();
            if (!nb.isValid()) continue;

            double lo = nb.getOrigin()[axis] - nb.getExtents()[axis];
            double hi = nb.getOrigin()[axis] + nb.getExtents()[axis];

            if (first)
            {
                refLo = lo;
                refHi = hi;
                first = false;
            }
            else if (std::abs(lo - refLo) > 1.0 || std::abs(hi - refHi) > 1.0)
            {
                allMatch = false;
                break;
            }
        }

        if (allMatch && !first)
            return axis;
    }

    Vector3 extents = combinedBounds.getExtents();
    if (extents.x() <= extents.y() && extents.x() <= extents.z()) return 0;
    if (extents.y() <= extents.x() && extents.y() <= extents.z()) return 1;
    return 2;
}

inline void sweepBrushesAlongPath(
    const std::vector<SourceBrushData>& sources,
    const AABB& sourceBounds,
    int sweepAxis,
    const std::vector<Vector3>& controlPoints,
    const SweepParams& params,
    const scene::INodePtr& parent)
{
    int numSamples = std::max(2, params.segments + 1);
    auto path = cables::sampleCatmullRomPath(controlPoints, numSamples);
    if (path.size() < 2) return;

    auto frames = cables::buildFramesAlongPath(path);

    Vector3 srcOrigin = sourceBounds.getOrigin();
    double sweepMid = sweepCoord(srcOrigin, sweepAxis);

    double texScale = 0.0078125;
    Matrix3 defaultProj = Matrix3::getIdentity();
    defaultProj.xx() = texScale;
    defaultProj.yy() = texScale;

    int N = static_cast<int>(frames.size()) - 1;

    for (int i = 0; i < N; ++i)
    {
        const cables::Frame& fStart = frames[i];
        const cables::Frame& fEnd = frames[i + 1];

        for (auto& src : sources)
        {
            auto brushNode = GlobalBrushCreator().createBrush();
            parent->addChildNode(brushNode);

            auto& brush = *Node_getIBrush(brushNode);

            for (auto& face : src.faces)
            {
                Vector3 p0 = transformSweepVertex(
                    face.points[0], srcOrigin, face.normal, sweepAxis, sweepMid, fStart, fEnd);
                Vector3 p1 = transformSweepVertex(
                    face.points[1], srcOrigin, face.normal, sweepAxis, sweepMid, fStart, fEnd);
                Vector3 p2 = transformSweepVertex(
                    face.points[2], srcOrigin, face.normal, sweepAxis, sweepMid, fStart, fEnd);

                Vector3 edge1 = p1 - p0;
                Vector3 edge2 = p2 - p0;
                Vector3 crossN = edge1.cross(edge2);

                double len = crossN.getLength();
                if (len < 1e-10) continue;
                crossN /= len;

                Vector3 expectedDir = approxTransformNormal(
                    face.normal, sweepAxis, fStart);
                if (crossN.dot(expectedDir) < 0)
                    crossN = -crossN;

                double dist = crossN.dot(p0);
                Plane3 transformed(crossN, dist);

                if (!transformed.isValid()) continue;
                brush.addFace(transformed, defaultProj, face.material);
            }

            brush.evaluateBRep();
            Node_setSelected(brushNode, true);
        }
    }
}

} // namespace sweep
