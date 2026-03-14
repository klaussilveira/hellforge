#pragma once

#include "ibrush.h"
#include "ipatch.h"
#include "iselection.h"
#include "scenelib.h"
#include "math/Vector3.h"
#include "math/AABB.h"
#include "math/pi.h"

#include <cmath>
#include <vector>
#include <algorithm>

namespace cornerpatch
{

struct CornerDetection
{
    bool valid = false;
    int depthAxis = 2;
    int axis1 = 0, axis2 = 1;
    double depthMin = 0;
    double depthMax = 0;

    Vector3 insideCorner;
    Vector3 dir1;
    Vector3 dir2;
    double radius1 = 0;
    double radius2 = 0;

    std::string faceMaterial;
};

inline CornerDetection detectCornerFromSelection()
{
    CornerDetection result;

    struct BrushInfo
    {
        AABB aabb;
        scene::INodePtr node;
    };

    std::vector<BrushInfo> brushes;
    GlobalSelectionSystem().foreachSelected([&](const scene::INodePtr& node) {
        if (Node_isBrush(node))
        {
            AABB bounds = node->worldAABB();
            if (bounds.isValid())
                brushes.push_back({bounds, node});
        }
    });

    if (brushes.size() != 2)
        return result;

    Vector3 aMin = brushes[0].aabb.getOrigin() - brushes[0].aabb.getExtents();
    Vector3 aMax = brushes[0].aabb.getOrigin() + brushes[0].aabb.getExtents();
    Vector3 bMin = brushes[1].aabb.getOrigin() - brushes[1].aabb.getExtents();
    Vector3 bMax = brushes[1].aabb.getOrigin() + brushes[1].aabb.getExtents();

    const double eps = 0.5;
    int axisCombos[3][3] = {{0, 1, 2}, {0, 2, 1}, {1, 2, 0}};

    for (auto& cfg : axisCombos)
    {
        int i = cfg[0];
        int j = cfg[1];
        int k = cfg[2];

        double overlapMin_k = std::max(aMin[k], bMin[k]);
        double overlapMax_k = std::min(aMax[k], bMax[k]);
        if (overlapMax_k - overlapMin_k < 1.0)
            continue;

        double combMin_i = std::min(aMin[i], bMin[i]);
        double combMax_i = std::max(aMax[i], bMax[i]);
        double combMin_j = std::min(aMin[j], bMin[j]);
        double combMax_j = std::max(aMax[j], bMax[j]);

        double overlapMin_i = std::max(aMin[i], bMin[i]);
        double overlapMax_i = std::min(aMax[i], bMax[i]);
        double overlapMin_j = std::max(aMin[j], bMin[j]);
        double overlapMax_j = std::min(aMax[j], bMax[j]);

        auto tryCorner = [&](double vi, double vj, double dir_i, double dir_j,
                             double r1, double r2,
                             double other_i_min, double other_i_max,
                             double other_j_min, double other_j_max) -> bool
        {
            if (vi < other_i_min - eps || vi > other_i_max + eps)
                return false;
            if (vj < other_j_min - eps || vj > other_j_max + eps)
                return false;
            if (vi <= combMin_i + eps || vi >= combMax_i - eps)
                return false;
            if (vj <= combMin_j + eps || vj >= combMax_j - eps)
                return false;

            result.valid = true;
            result.depthAxis = k;
            result.axis1 = i;
            result.axis2 = j;
            result.depthMin = overlapMin_k;
            result.depthMax = overlapMax_k;

            result.insideCorner = Vector3(0, 0, 0);
            result.insideCorner[i] = vi;
            result.insideCorner[j] = vj;
            result.insideCorner[k] = overlapMin_k;

            result.dir1 = Vector3(0, 0, 0);
            result.dir1[i] = dir_i;
            result.dir2 = Vector3(0, 0, 0);
            result.dir2[j] = dir_j;

            result.radius1 = r1;
            result.radius2 = r2;

            return true;
        };

        double aThick_i = aMax[i] - aMin[i];
        double aThick_j = aMax[j] - aMin[j];
        double bThick_i = bMax[i] - bMin[i];
        double bThick_j = bMax[j] - bMin[j];

        for (int ai = 0; ai < 2; ++ai)
        {
            double vi = (ai == 0) ? aMin[i] : aMax[i];
            double dir_i = (ai == 0) ? -1.0 : 1.0;

            for (int bj = 0; bj < 2; ++bj)
            {
                double vj = (bj == 0) ? bMin[j] : bMax[j];
                double dir_j = (bj == 0) ? -1.0 : 1.0;

                if (tryCorner(vi, vj, dir_i, dir_j, aThick_i, bThick_j,
                              bMin[i], bMax[i], aMin[j], aMax[j]))
                    goto found;
            }
        }

        for (int bi = 0; bi < 2; ++bi)
        {
            double vi = (bi == 0) ? bMin[i] : bMax[i];
            double dir_i = (bi == 0) ? -1.0 : 1.0;

            for (int aj = 0; aj < 2; ++aj)
            {
                double vj = (aj == 0) ? aMin[j] : aMax[j];
                double dir_j = (aj == 0) ? -1.0 : 1.0;

                if (tryCorner(vi, vj, dir_i, dir_j, bThick_i, aThick_j,
                              aMin[i], aMax[i], bMin[j], bMax[j]))
                    goto found;
            }
        }

        continue;
        found:
        {
            auto* brush = Node_getIBrush(brushes[0].node);
            if (brush)
            {
                double bestDot = -2.0;
                for (std::size_t fi = 0; fi < brush->getNumFaces(); ++fi)
                {
                    const auto& face = brush->getFace(fi);
                    Vector3 normal = face.getPlane3().normal();
                    double d1 = std::abs(normal.dot(result.dir1));
                    double d2 = std::abs(normal.dot(result.dir2));
                    double best = std::max(d1, d2);
                    if (best > bestDot)
                    {
                        bestDot = best;
                        result.faceMaterial = face.getShader();
                    }
                }
            }

            return result;
        }
    }

    return result;
}

inline void generateCornerPatch(
    const CornerDetection& corner, int segments,
    double radius, double arcDegrees, bool invert,
    const scene::INodePtr& parent)
{
    if (!corner.valid || segments < 1)
        return;

    double r1 = (radius > 0) ? radius : corner.radius1;
    double r2 = (radius > 0) ? radius : corner.radius2;
    double arcRadians = arcDegrees * math::PI / 180.0;

    Vector3 arcCenter = corner.insideCorner;
    Vector3 d1 = corner.dir1;
    Vector3 d2 = corner.dir2;

    if (invert)
    {
        arcCenter = corner.insideCorner + d1 * r1 + d2 * r2;
        d1 = corner.dir1 * -1.0;
        d2 = corner.dir2 * -1.0;
        std::swap(d1, d2);
        std::swap(r1, r2);
    }

    int patchWidth = 2 * segments + 1;
    int patchHeight = 3;

    auto patchNode = GlobalPatchModule().createPatch(patch::PatchDefType::Def2);
    parent->addChildNode(patchNode);

    auto* patch = Node_getIPatch(patchNode);
    patch->setDims(static_cast<std::size_t>(patchWidth),
                   static_cast<std::size_t>(patchHeight));
    patch->setShader(corner.faceMaterial);

    double anglePerSeg = arcRadians / segments;

    for (int row = 0; row < patchHeight; ++row)
    {
        double depthT = static_cast<double>(row) / (patchHeight - 1);
        double depth = corner.depthMin + (corner.depthMax - corner.depthMin) * depthT;

        Vector3 center = arcCenter;
        center[corner.depthAxis] = depth;

        for (int col = 0; col < patchWidth; ++col)
        {
            int segIdx = col / 2;
            bool isOnCurve = (col % 2 == 0);

            Vector3 pos;

            if (isOnCurve)
            {
                double theta = segIdx * anglePerSeg;
                pos = center +
                      d1 * (r1 * std::cos(theta)) +
                      d2 * (r2 * std::sin(theta));
            }
            else
            {
                double thetaStart = segIdx * anglePerSeg;
                double tanHalf = std::tan(anglePerSeg / 2.0);

                Vector3 pStart = center +
                    d1 * (r1 * std::cos(thetaStart)) +
                    d2 * (r2 * std::sin(thetaStart));

                Vector3 tangent =
                    d1 * (-r1 * std::sin(thetaStart)) +
                    d2 * (r2 * std::cos(thetaStart));

                pos = pStart + tangent * tanHalf;
            }

            patch->ctrlAt(row, col).vertex = pos;
        }
    }

    patch->controlPointsChanged();
    patch->scaleTextureNaturally();
    Node_setSelected(patchNode, true);
}

} // namespace cornerpatch
