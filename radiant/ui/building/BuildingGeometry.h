#pragma once

#include "ibrush.h"
#include "scenelib.h"
#include "math/Plane3.h"
#include "math/Matrix3.h"
#include "math/Vector3.h"

#include <cmath>
#include <algorithm>
#include <string>

namespace building
{

struct BuildingParams
{
    int floorCount = 3;
    double floorHeight = 0;
    double wallThickness = 8;
    double trimHeight = 8;
    int windowsPerFloor = 0;
    double windowWidth = 48;
    double windowHeight = 56;
    double sillHeight = 32;
    int roofType = 0;
    double roofHeight = 64;
    double roofBorderHeight = 16;
    bool cornerColumns = false;
    double cornerExtrude = 0;
    std::string wallMaterial = "_default";
    std::string trimMaterial = "_default";
};

inline scene::INodePtr createBoxBrush(
    const Vector3& mins, const Vector3& maxs,
    const std::string& material, const scene::INodePtr& parent)
{
    if (maxs.x() - mins.x() < 1 || maxs.y() - mins.y() < 1 || maxs.z() - mins.z() < 1)
        return scene::INodePtr();

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

inline int computeAutoWindowCount(double wallLength, double windowWidth)
{
    if (windowWidth <= 0 || wallLength <= 0) {
        return 0;
    }

    double targetSpacing = windowWidth * 2.5;
    int count = static_cast<int>(wallLength / targetSpacing);
    if (count < 1 && wallLength >= windowWidth * 1.5) {
        count = 1;
    }

    while (count > 0 && windowWidth * count > wallLength * 0.8) {
        --count;
    }

    return std::max(0, count);
}

inline void generateWallWithWindows(
    const Vector3& wallMins, const Vector3& wallMaxs,
    int windowAxis, int windowCount,
    double windowWidth, double windowHeight, double sillHeight,
    const std::string& material, const scene::INodePtr& parent)
{
    double wallLength = wallMaxs[windowAxis] - wallMins[windowAxis];
    double wallHeight = wallMaxs.z() - wallMins.z();

    if (windowCount <= 0 || windowWidth <= 0 || windowHeight <= 0 ||
        windowHeight + sillHeight > wallHeight ||
        windowWidth * windowCount > wallLength)
    {
        auto node = createBoxBrush(wallMins, wallMaxs, material, parent);
        if (node) Node_setSelected(node, true);
        return;
    }

    double spacing = wallLength / windowCount;
    double windowBottom = wallMins.z() + sillHeight;
    double windowTop = windowBottom + windowHeight;

    double prevEdge = wallMins[windowAxis];

    for (int w = 0; w < windowCount; ++w)
    {
        double winCenter = wallMins[windowAxis] + (w + 0.5) * spacing;
        double winLeft = winCenter - windowWidth / 2.0;
        double winRight = winCenter + windowWidth / 2.0;

        if (winLeft > prevEdge + 0.5)
        {
            Vector3 colMins = wallMins;
            Vector3 colMaxs = wallMaxs;
            colMins[windowAxis] = prevEdge;
            colMaxs[windowAxis] = winLeft;
            auto node = createBoxBrush(colMins, colMaxs, material, parent);
            if (node) Node_setSelected(node, true);
        }

        if (sillHeight > 0.5)
        {
            Vector3 sMins = wallMins;
            Vector3 sMaxs = wallMaxs;
            sMins[windowAxis] = winLeft;
            sMaxs[windowAxis] = winRight;
            sMaxs.z() = windowBottom;
            auto node = createBoxBrush(sMins, sMaxs, material, parent);
            if (node) Node_setSelected(node, true);
        }

        if (windowTop < wallMaxs.z() - 0.5)
        {
            Vector3 hMins = wallMins;
            Vector3 hMaxs = wallMaxs;
            hMins[windowAxis] = winLeft;
            hMaxs[windowAxis] = winRight;
            hMins.z() = windowTop;
            auto node = createBoxBrush(hMins, hMaxs, material, parent);
            if (node) Node_setSelected(node, true);
        }

        prevEdge = winRight;
    }

    if (wallMaxs[windowAxis] > prevEdge + 0.5)
    {
        Vector3 colMins = wallMins;
        Vector3 colMaxs = wallMaxs;
        colMins[windowAxis] = prevEdge;
        colMaxs[windowAxis] = wallMaxs[windowAxis];
        auto node = createBoxBrush(colMins, colMaxs, material, parent);
        if (node) Node_setSelected(node, true);
    }
}

inline void generateBuilding(
    const Vector3& mins, const Vector3& maxs,
    const BuildingParams& params, const scene::INodePtr& parent)
{
    double floorHeight = params.floorHeight;
    if (floorHeight <= 0)
        floorHeight = (maxs.z() - mins.z()) / params.floorCount;

    double t = params.wallThickness;

    double ix0 = mins.x() + t;
    double ix1 = maxs.x() - t;

    double iy0 = params.cornerColumns ? mins.y() + t : mins.y();
    double iy1 = params.cornerColumns ? maxs.y() - t : maxs.y();

    double eastWestLen = iy1 - iy0;
    double northSouthLen = (maxs.x() - mins.x()) - 2 * t;

    if (params.cornerColumns)
    {
        double e = params.cornerExtrude;
        double colTop = mins.z() + params.floorCount * floorHeight + params.trimHeight;
        auto c0 = createBoxBrush(Vector3(mins.x() - e, maxs.y() - t, mins.z()), Vector3(mins.x() + t, maxs.y() + e, colTop), params.trimMaterial, parent);
        if (c0) Node_setSelected(c0, true);
        auto c1 = createBoxBrush(Vector3(maxs.x() - t, maxs.y() - t, mins.z()), Vector3(maxs.x() + e, maxs.y() + e, colTop), params.trimMaterial, parent);
        if (c1) Node_setSelected(c1, true);
        auto c2 = createBoxBrush(Vector3(mins.x() - e, mins.y() - e, mins.z()), Vector3(mins.x() + t, mins.y() + t, colTop), params.trimMaterial, parent);
        if (c2) Node_setSelected(c2, true);
        auto c3 = createBoxBrush(Vector3(maxs.x() - t, mins.y() - e, mins.z()), Vector3(maxs.x() + e, mins.y() + t, colTop), params.trimMaterial, parent);
        if (c3) Node_setSelected(c3, true);
    }

    for (int floor = 0; floor < params.floorCount; ++floor)
    {
        double fz0 = mins.z() + floor * floorHeight;
        double fz1 = fz0 + floorHeight;
        double trimTop = fz0 + params.trimHeight;
        double wallBot = trimTop;
        double wallTop = fz1;

        {
            Vector3 slabMins(mins.x(), mins.y(), fz0);
            Vector3 slabMaxs(maxs.x(), maxs.y(), trimTop);
            auto node = createBoxBrush(slabMins, slabMaxs, params.trimMaterial, parent);
            if (node) Node_setSelected(node, true);
        }

        double wallH = wallTop - wallBot;
        if (wallH < 1) continue;

        // -1 = no windows, 0 = auto, >0 = manual count
        int ewWindows = 0;
        int nsWindows = 0;
        if (params.windowsPerFloor == 0)
        {
            ewWindows = computeAutoWindowCount(eastWestLen, params.windowWidth);
            nsWindows = computeAutoWindowCount(northSouthLen, params.windowWidth);
        }
        else if (params.windowsPerFloor > 0)
        {
            ewWindows = params.windowsPerFloor;
            nsWindows = params.windowsPerFloor;
        }

        generateWallWithWindows(
            Vector3(maxs.x() - t, iy0, wallBot),
            Vector3(maxs.x(), iy1, wallTop),
            1, ewWindows,
            params.windowWidth, params.windowHeight, params.sillHeight,
            params.wallMaterial, parent);

        generateWallWithWindows(
            Vector3(mins.x(), iy0, wallBot),
            Vector3(mins.x() + t, iy1, wallTop),
            1, ewWindows,
            params.windowWidth, params.windowHeight, params.sillHeight,
            params.wallMaterial, parent);

        if (northSouthLen > 0)
        {
            generateWallWithWindows(
                Vector3(ix0, maxs.y() - t, wallBot),
                Vector3(ix1, maxs.y(), wallTop),
                0, nsWindows,
                params.windowWidth, params.windowHeight, params.sillHeight,
                params.wallMaterial, parent);

            generateWallWithWindows(
                Vector3(ix0, mins.y(), wallBot),
                Vector3(ix1, mins.y() + t, wallTop),
                0, nsWindows,
                params.windowWidth, params.windowHeight, params.sillHeight,
                params.wallMaterial, parent);
        }
    }

    double topZ = mins.z() + params.floorCount * floorHeight;
    {
        Vector3 slabMins(mins.x(), mins.y(), topZ);
        Vector3 slabMaxs(maxs.x(), maxs.y(), topZ + params.trimHeight);
        auto node = createBoxBrush(slabMins, slabMaxs, params.trimMaterial, parent);
        if (node) Node_setSelected(node, true);
    }

    double roofBaseZ = topZ + params.trimHeight;
    double texScale = 0.0078125;
    Matrix3 proj = Matrix3::getIdentity();
    proj.xx() = texScale;
    proj.yy() = texScale;

    switch (params.roofType)
    {
    case 0:
        break;
    case 1:
    {
        double bh = params.roofBorderHeight;
        auto n = createBoxBrush(
            Vector3(mins.x(), maxs.y() - t, roofBaseZ),
            Vector3(maxs.x(), maxs.y(), roofBaseZ + bh),
            params.trimMaterial, parent);
        if (n) Node_setSelected(n, true);

        auto s = createBoxBrush(
            Vector3(mins.x(), mins.y(), roofBaseZ),
            Vector3(maxs.x(), mins.y() + t, roofBaseZ + bh),
            params.trimMaterial, parent);
        if (s) Node_setSelected(s, true);

        auto e = createBoxBrush(
            Vector3(maxs.x() - t, mins.y() + t, roofBaseZ),
            Vector3(maxs.x(), maxs.y() - t, roofBaseZ + bh),
            params.trimMaterial, parent);
        if (e) Node_setSelected(e, true);

        auto w = createBoxBrush(
            Vector3(mins.x(), mins.y() + t, roofBaseZ),
            Vector3(mins.x() + t, maxs.y() - t, roofBaseZ + bh),
            params.trimMaterial, parent);
        if (w) Node_setSelected(w, true);
        break;
    }
    case 2:
    {
        double rh = params.roofHeight;
        double dy = maxs.y() - mins.y();
        double len = std::sqrt(rh * rh + dy * dy);

        auto brushNode = GlobalBrushCreator().createBrush();
        parent->addChildNode(brushNode);
        auto& brush = *Node_getIBrush(brushNode);

        brush.addFace(Plane3(1, 0, 0, maxs.x()), proj, params.wallMaterial);
        brush.addFace(Plane3(-1, 0, 0, -mins.x()), proj, params.wallMaterial);
        brush.addFace(Plane3(0, 1, 0, maxs.y()), proj, params.wallMaterial);
        brush.addFace(Plane3(0, -1, 0, -mins.y()), proj, params.wallMaterial);
        brush.addFace(Plane3(0, 0, -1, -roofBaseZ), proj, params.wallMaterial);

        double ny = -rh / len, nz = dy / len;
        double dist = ny * maxs.y() + nz * (roofBaseZ + rh);
        brush.addFace(Plane3(0, ny, nz, dist), proj, params.wallMaterial);

        brush.evaluateBRep();
        Node_setSelected(brushNode, true);
        break;
    }
    case 3:
    {
        double rh = params.roofHeight;
        double midY = (mins.y() + maxs.y()) / 2.0;
        double halfY = (maxs.y() - mins.y()) / 2.0;
        double len = std::sqrt(rh * rh + halfY * halfY);

        {
            auto brushNode = GlobalBrushCreator().createBrush();
            parent->addChildNode(brushNode);
            auto& brush = *Node_getIBrush(brushNode);

            brush.addFace(Plane3(1, 0, 0, maxs.x()), proj, params.wallMaterial);
            brush.addFace(Plane3(-1, 0, 0, -mins.x()), proj, params.wallMaterial);
            brush.addFace(Plane3(0, 1, 0, maxs.y()), proj, params.wallMaterial);
            brush.addFace(Plane3(0, -1, 0, -midY), proj, params.wallMaterial);
            brush.addFace(Plane3(0, 0, -1, -roofBaseZ), proj, params.wallMaterial);

            double ny = rh / len, nz = halfY / len;
            double dist = ny * maxs.y() + nz * roofBaseZ;
            brush.addFace(Plane3(0, ny, nz, dist), proj, params.wallMaterial);

            brush.evaluateBRep();
            Node_setSelected(brushNode, true);
        }

        {
            auto brushNode = GlobalBrushCreator().createBrush();
            parent->addChildNode(brushNode);
            auto& brush = *Node_getIBrush(brushNode);

            brush.addFace(Plane3(1, 0, 0, maxs.x()), proj, params.wallMaterial);
            brush.addFace(Plane3(-1, 0, 0, -mins.x()), proj, params.wallMaterial);
            brush.addFace(Plane3(0, 1, 0, midY), proj, params.wallMaterial);
            brush.addFace(Plane3(0, -1, 0, -mins.y()), proj, params.wallMaterial);
            brush.addFace(Plane3(0, 0, -1, -roofBaseZ), proj, params.wallMaterial);

            double ny = -rh / len, nz = halfY / len;
            double dist = ny * mins.y() + nz * roofBaseZ;
            brush.addFace(Plane3(0, ny, nz, dist), proj, params.wallMaterial);

            brush.evaluateBRep();
            Node_setSelected(brushNode, true);
        }
        break;
    }
    }
}

} // namespace building
