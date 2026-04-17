#pragma once

#include "ibrush.h"
#include "scenelib.h"
#include "math/Plane3.h"
#include "math/Matrix3.h"
#include "math/Vector3.h"

#include <cmath>
#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

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
    bool noFirstFloorWindows = false;
    bool firstFloorDoor = false;
    double doorWidth = 48;
    double doorHeight = 96;
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

inline void generateWallWithDoor(
    const Vector3& wallMins, const Vector3& wallMaxs,
    int doorAxis, double doorWidth, double doorHeight,
    const std::string& material, const scene::INodePtr& parent)
{
    double wallLen = wallMaxs[doorAxis] - wallMins[doorAxis];
    double wallH = wallMaxs.z() - wallMins.z();

    if (doorWidth <= 0 || doorHeight <= 0 ||
        doorWidth >= wallLen - 1 || doorHeight >= wallH - 1)
    {
        auto n = createBoxBrush(wallMins, wallMaxs, material, parent);
        if (n) Node_setSelected(n, true);
        return;
    }

    double center = wallMins[doorAxis] + wallLen * 0.5;
    double doorLeft = center - doorWidth * 0.5;
    double doorRight = center + doorWidth * 0.5;
    double doorTop = wallMins.z() + doorHeight;

    {
        Vector3 mn = wallMins, mx = wallMaxs;
        mx[doorAxis] = doorLeft;
        auto n = createBoxBrush(mn, mx, material, parent);
        if (n) Node_setSelected(n, true);
    }
    {
        Vector3 mn = wallMins, mx = wallMaxs;
        mn[doorAxis] = doorRight;
        auto n = createBoxBrush(mn, mx, material, parent);
        if (n) Node_setSelected(n, true);
    }
    if (doorTop < wallMaxs.z() - 0.5)
    {
        Vector3 mn = wallMins, mx = wallMaxs;
        mn[doorAxis] = doorLeft;
        mx[doorAxis] = doorRight;
        mn.z() = doorTop;
        auto n = createBoxBrush(mn, mx, material, parent);
        if (n) Node_setSelected(n, true);
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

        if (floor == 0 && params.noFirstFloorWindows)
        {
            ewWindows = 0;
            nsWindows = 0;
        }

        bool firstFloorDoor = (floor == 0 && params.firstFloorDoor && northSouthLen > 0);

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

            if (firstFloorDoor)
            {
                generateWallWithDoor(
                    Vector3(ix0, mins.y(), wallBot),
                    Vector3(ix1, mins.y() + t, wallTop),
                    0, params.doorWidth, params.doorHeight,
                    params.wallMaterial, parent);
            }
            else
            {
                generateWallWithWindows(
                    Vector3(ix0, mins.y(), wallBot),
                    Vector3(ix1, mins.y() + t, wallTop),
                    0, nsWindows,
                    params.windowWidth, params.windowHeight, params.sillHeight,
                    params.wallMaterial, parent);
            }
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

struct MaskedFootprint
{
    int cols = 0;
    int rows = 0;
    double tileW = 0;
    double tileH = 0;
    Vector3 origin;
    std::vector<std::vector<bool>> mask;
    // Optional, same dims as mask. Per-cell bitmask of externally occluded sides:
    // bit 0 = +Y (N), bit 1 = -Y (S), bit 2 = +X (E), bit 3 = -X (W).
    std::vector<std::vector<uint8_t>> occlusion;
};

inline bool maskIsSingleRect(
    const std::vector<std::vector<bool>>& mask, int cols, int rows,
    int& x0, int& y0, int& x1, int& y1)
{
    x0 = cols; y0 = rows; x1 = -1; y1 = -1;
    int count = 0;
    for (int r = 0; r < rows; ++r)
        for (int c = 0; c < cols; ++c)
            if (mask[r][c])
            {
                x0 = std::min(x0, c); y0 = std::min(y0, r);
                x1 = std::max(x1, c); y1 = std::max(y1, r);
                ++count;
            }
    if (count == 0) return false;
    int w = x1 - x0 + 1, h = y1 - y0 + 1;
    return count == w * h;
}

struct MaskRect { int x0, y0, x1, y1; };

inline std::vector<MaskRect> mergeMaskRects(
    const std::vector<std::vector<bool>>& mask, int cols, int rows)
{
    std::vector<std::vector<bool>> visited(rows, std::vector<bool>(cols, false));
    std::vector<MaskRect> rects;

    for (int y = 0; y < rows; ++y)
    {
        for (int x = 0; x < cols; ++x)
        {
            if (visited[y][x] || !mask[y][x]) continue;

            int maxX = x;
            while (maxX + 1 < cols && !visited[y][maxX + 1] && mask[y][maxX + 1])
                ++maxX;

            int maxY = y;
            bool canExpand = true;
            while (canExpand && maxY + 1 < rows)
            {
                for (int cx = x; cx <= maxX; ++cx)
                    if (visited[maxY + 1][cx] || !mask[maxY + 1][cx])
                    {
                        canExpand = false;
                        break;
                    }
                if (canExpand) ++maxY;
            }

            for (int fy = y; fy <= maxY; ++fy)
                for (int fx = x; fx <= maxX; ++fx)
                    visited[fy][fx] = true;

            rects.push_back({x, y, maxX, maxY});
        }
    }

    return rects;
}

// A horizontal run spans columns [c0..c1] at the edge on row boundary `fixed`.
// `dir` = +1 means outward normal +Y; -1 means -Y.
// A vertical run spans rows [r0..r1] at the edge on column boundary `fixed`.
// `dir` = +1 means outward normal +X; -1 means -X.
struct PerimeterRun
{
    int axis;   // 0 = horizontal (constant-Y wall, spans X), 1 = vertical
    int dir;    // +1 or -1
    int fixed;  // row boundary (for axis=0) or column boundary (for axis=1)
    int start;  // start cell index along the run
    int endIncl; // end cell index, inclusive
};

inline std::vector<PerimeterRun> traceMaskPerimeter(
    const std::vector<std::vector<bool>>& mask, int cols, int rows)
{
    std::vector<PerimeterRun> runs;

    auto outside = [&](int c, int r) {
        if (c < 0 || c >= cols || r < 0 || r >= rows) return true;
        return !mask[r][c];
    };

    for (int r = 0; r < rows; ++r)
    {
        int c = 0;
        while (c < cols)
        {
            if (!mask[r][c] || !outside(c, r + 1)) { ++c; continue; }
            int start = c;
            while (c < cols && mask[r][c] && outside(c, r + 1)) ++c;
            runs.push_back({0, +1, r + 1, start, c - 1});
        }
    }

    for (int r = 0; r < rows; ++r)
    {
        int c = 0;
        while (c < cols)
        {
            if (!mask[r][c] || !outside(c, r - 1)) { ++c; continue; }
            int start = c;
            while (c < cols && mask[r][c] && outside(c, r - 1)) ++c;
            runs.push_back({0, -1, r, start, c - 1});
        }
    }

    for (int c = 0; c < cols; ++c)
    {
        int r = 0;
        while (r < rows)
        {
            if (!mask[r][c] || !outside(c + 1, r)) { ++r; continue; }
            int start = r;
            while (r < rows && mask[r][c] && outside(c + 1, r)) ++r;
            runs.push_back({1, +1, c + 1, start, r - 1});
        }
    }

    for (int c = 0; c < cols; ++c)
    {
        int r = 0;
        while (r < rows)
        {
            if (!mask[r][c] || !outside(c - 1, r)) { ++r; continue; }
            int start = r;
            while (r < rows && mask[r][c] && outside(c - 1, r)) ++r;
            runs.push_back({1, -1, c, start, r - 1});
        }
    }

    return runs;
}

inline void generateBuilding(const MaskedFootprint& fp,
    const BuildingParams& paramsIn, const scene::INodePtr& parent)
{
    int x0, y0, x1, y1;
    bool isRect = maskIsSingleRect(fp.mask, fp.cols, fp.rows, x0, y0, x1, y1);

    double floorHeight = paramsIn.floorHeight > 0 ? paramsIn.floorHeight : 128.0;
    double totalHeight = paramsIn.floorCount * floorHeight;

    bool hasOcclusion = !fp.occlusion.empty();

    if (isRect && !hasOcclusion)
    {
        Vector3 mins(
            fp.origin.x() + x0 * fp.tileW,
            fp.origin.y() + y0 * fp.tileH,
            fp.origin.z());
        Vector3 maxs(
            fp.origin.x() + (x1 + 1) * fp.tileW,
            fp.origin.y() + (y1 + 1) * fp.tileH,
            fp.origin.z() + totalHeight);
        generateBuilding(mins, maxs, paramsIn, parent);
        return;
    }

    BuildingParams params = paramsIn;
    if (!isRect || hasOcclusion)
    {
        if (params.roofType == 2 || params.roofType == 3)
            params.roofType = 0;
    }
    if (!isRect)
        params.cornerColumns = false;

    double t = params.wallThickness;

    auto footprintRects = mergeMaskRects(fp.mask, fp.cols, fp.rows);
    auto perimeter = traceMaskPerimeter(fp.mask, fp.cols, fp.rows);

    int doorRunIdx = -1;
    if (params.firstFloorDoor && !perimeter.empty())
    {
        int bestLen = -1;
        for (size_t i = 0; i < perimeter.size(); ++i)
        {
            int len = perimeter[i].endIncl - perimeter[i].start + 1;
            if (len > bestLen) { bestLen = len; doorRunIdx = static_cast<int>(i); }
        }
    }

    auto rectBox = [&](const MaskRect& r, double zMin, double zMax) {
        Vector3 mn(fp.origin.x() + r.x0 * fp.tileW,
                   fp.origin.y() + r.y0 * fp.tileH, zMin);
        Vector3 mx(fp.origin.x() + (r.x1 + 1) * fp.tileW,
                   fp.origin.y() + (r.y1 + 1) * fp.tileH, zMax);
        return std::make_pair(mn, mx);
    };

    auto cellOutside = [&](int c, int r) {
        if (c < 0 || c >= fp.cols || r < 0 || r >= fp.rows) return true;
        return !fp.mask[r][c];
    };

    auto subRunWorldBox = [&](const PerimeterRun& run, int cStart, int cEnd,
                              double zBot, double zTop) {
        Vector3 mn, mx;
        if (run.axis == 0)
        {
            int interiorRow = (run.dir > 0) ? run.fixed - 1 : run.fixed;
            double y = fp.origin.y() + run.fixed * fp.tileH;
            double x0w = fp.origin.x() + cStart * fp.tileW;
            double x1w = fp.origin.x() + (cEnd + 1) * fp.tileW;

            if (cStart == run.start && cellOutside(run.start - 1, interiorRow)) x0w += t;
            if (cEnd == run.endIncl && cellOutside(run.endIncl + 1, interiorRow)) x1w -= t;

            if (run.dir > 0) { mn = Vector3(x0w, y - t, zBot); mx = Vector3(x1w, y,     zTop); }
            else             { mn = Vector3(x0w, y,     zBot); mx = Vector3(x1w, y + t, zTop); }
        }
        else
        {
            double x = fp.origin.x() + run.fixed * fp.tileW;
            double y0w = fp.origin.y() + cStart * fp.tileH;
            double y1w = fp.origin.y() + (cEnd + 1) * fp.tileH;
            if (run.dir > 0) { mn = Vector3(x - t, y0w, zBot); mx = Vector3(x,     y1w, zTop); }
            else             { mn = Vector3(x,     y0w, zBot); mx = Vector3(x + t, y1w, zTop); }
        }
        return std::make_pair(mn, mx);
    };

    auto runWorldBox = [&](const PerimeterRun& run, double zBot, double zTop) {
        return subRunWorldBox(run, run.start, run.endIncl, zBot, zTop);
    };

    auto cellOccluded = [&](const PerimeterRun& run, int cellIdx) -> bool {
        if (fp.occlusion.empty()) return false;
        int cc, rr, bit;
        if (run.axis == 0)
        {
            int interiorRow = (run.dir > 0) ? run.fixed - 1 : run.fixed;
            rr = interiorRow; cc = cellIdx;
            bit = (run.dir > 0) ? 0 : 1;
        }
        else
        {
            int interiorCol = (run.dir > 0) ? run.fixed - 1 : run.fixed;
            cc = interiorCol; rr = cellIdx;
            bit = (run.dir > 0) ? 2 : 3;
        }
        if (rr < 0 || rr >= static_cast<int>(fp.occlusion.size())) return false;
        if (cc < 0 || cc >= static_cast<int>(fp.occlusion[rr].size())) return false;
        return (fp.occlusion[rr][cc] & (1 << bit)) != 0;
    };

    for (int f = 0; f < params.floorCount; ++f)
    {
        double fz0 = fp.origin.z() + f * floorHeight;
        double fz1 = fz0 + floorHeight;
        double trimTop = fz0 + params.trimHeight;
        double wallBot = trimTop;
        double wallTop = fz1;

        for (const auto& r : footprintRects)
        {
            auto box = rectBox(r, fz0, trimTop);
            auto n = createBoxBrush(box.first, box.second, params.trimMaterial, parent);
            if (n) Node_setSelected(n, true);
        }

        if (wallTop - wallBot < 1) continue;

        auto windowsForCells = [&](int axis, int cellCount) -> int {
            if (params.windowsPerFloor == -1) return 0;
            if (params.windowsPerFloor > 0) return params.windowsPerFloor;
            double tileSize = (axis == 0) ? fp.tileW : fp.tileH;
            double spacing = params.windowWidth * 2.5;
            if (spacing <= 0 || tileSize <= 0) return 0;
            int perTile = static_cast<int>(std::round(tileSize / spacing));
            if (perTile < 1 && tileSize >= params.windowWidth * 1.5) perTile = 1;
            perTile = std::max(0, perTile);
            return cellCount * perTile;
        };

        for (size_t i = 0; i < perimeter.size(); ++i)
        {
            const auto& run = perimeter[i];

            if (f == 0 && static_cast<int>(i) == doorRunIdx)
            {
                auto box = runWorldBox(run, wallBot, wallTop);
                generateWallWithDoor(
                    box.first, box.second,
                    run.axis == 0 ? 0 : 1,
                    params.doorWidth, params.doorHeight,
                    params.wallMaterial, parent);
                continue;
            }

            int subStart = run.start;
            bool prevOcc = cellOccluded(run, run.start);
            for (int cell = run.start + 1; cell <= run.endIncl + 1; ++cell)
            {
                bool atEnd = (cell > run.endIncl);
                bool currOcc = atEnd ? !prevOcc : cellOccluded(run, cell);
                if (currOcc != prevOcc)
                {
                    int subEnd = cell - 1;
                    auto box = subRunWorldBox(run, subStart, subEnd, wallBot, wallTop);

                    int cellCount = subEnd - subStart + 1;
                    bool noWin = (f == 0 && params.noFirstFloorWindows) || prevOcc;
                    int winCount = noWin ? 0 : windowsForCells(run.axis, cellCount);

                    generateWallWithWindows(
                        box.first, box.second,
                        run.axis == 0 ? 0 : 1,
                        winCount,
                        params.windowWidth, params.windowHeight, params.sillHeight,
                        params.wallMaterial, parent);

                    subStart = cell;
                    prevOcc = currOcc;
                }
            }
        }
    }

    double topZ = fp.origin.z() + params.floorCount * floorHeight;
    for (const auto& r : footprintRects)
    {
        auto box = rectBox(r, topZ, topZ + params.trimHeight);
        auto n = createBoxBrush(box.first, box.second, params.trimMaterial, parent);
        if (n) Node_setSelected(n, true);
    }

    double roofBaseZ = topZ + params.trimHeight;
    if (params.roofType == 1)
    {
        double bh = params.roofBorderHeight;
        for (const auto& run : perimeter)
        {
            auto box = runWorldBox(run, roofBaseZ, roofBaseZ + bh);
            auto n = createBoxBrush(box.first, box.second, params.trimMaterial, parent);
            if (n) Node_setSelected(n, true);
        }
    }
}

} // namespace building
