#pragma once

#include "ibrush.h"
#include "scenelib.h"
#include "math/Plane3.h"
#include "math/Matrix3.h"
#include "math/Vector3.h"
#include "math/AABB.h"

#include <cmath>
#include <vector>
#include <algorithm>

namespace building
{

struct BuildingParams
{
    // Floor
    int floorCount;
    float floorHeight;
    float floorThickness;
    float trimHeight;
    float wallThickness;

    // Windows
    int windowMode;      // 0=auto, 1=manual
    int windowsPerWall;
    float windowWidth;
    float windowHeight;
    float windowSillHeight;
    float windowInset;

    // Roof
    int roofType;        // 0=flat, 1=flat+border, 2=slanted, 3=A-frame
    float roofBorderHeight;
    float roofSlopeHeight;
    int roofSlopeDirection;
    float aRoofHeight;
    int aRoofDirection;

    // Materials
    std::string wallMaterial;
    std::string trimMaterial;
    std::string windowFrameMaterial;
};

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

// Calculate how many windows fit on a wall of the given length
inline int calcAutoWindowCount(double wallLength, double wallThickness, double windowWidth)
{
    double usable = wallLength - 2.0 * wallThickness;
    if (usable <= 0 || windowWidth <= 0)
        return 0;

    // Each window needs its width plus at least the same width as spacing on each side
    // Use a minimum pillar width of windowWidth * 0.5 between windows
    double pillarWidth = std::max(windowWidth * 0.5, wallThickness);
    int count = static_cast<int>((usable + pillarWidth) / (windowWidth + pillarWidth));
    return std::max(count, 0);
}

// Generate wall segments for one side, with window cutouts.
// wallMins/wallMaxs define the full wall slab.
// axis: 0 = wall runs along X, 1 = wall runs along Y.
// windowCount: number of windows to cut.
inline void generateWallWithWindows(
    const Vector3& wallMins, const Vector3& wallMaxs,
    int axis, int windowCount,
    double windowWidth, double windowHeight, double sillHeight, double windowInset,
    const std::string& wallMaterial, const std::string& frameMaterial,
    const scene::INodePtr& parent)
{
    if (windowCount <= 0)
    {
        // Solid wall, no windows
        auto node = createBoxBrush(wallMins, wallMaxs, wallMaterial, parent);
        Node_setSelected(node, true);
        return;
    }

    double wallLength = (axis == 0)
        ? (wallMaxs.x() - wallMins.x())
        : (wallMaxs.y() - wallMins.y());

    double wallStart = (axis == 0) ? wallMins.x() : wallMins.y();

    // Distribute windows evenly with equal spacing
    double totalWindowWidth = windowCount * windowWidth;
    double totalSpacing = wallLength - totalWindowWidth;
    double spacing = totalSpacing / (windowCount + 1);

    double windowBot = wallMins.z() + sillHeight;
    double windowTop = windowBot + windowHeight;

    // Clamp window top to wall top
    if (windowTop > wallMaxs.z())
        windowTop = wallMaxs.z();
    if (windowBot >= windowTop)
    {
        // Windows don't fit vertically, just make a solid wall
        auto node = createBoxBrush(wallMins, wallMaxs, wallMaterial, parent);
        Node_setSelected(node, true);
        return;
    }

    // We build the wall as segments:
    // For each window: pillar before it, sill below, lintel above, and optionally inset pieces.
    // Then the final pillar after the last window.

    // Simpler approach: create the wall in vertical strips (pillars between windows)
    // and horizontal strips (sill row, lintel row) for each window bay.

    double cursor = wallStart;

    for (int w = 0; w <= windowCount; ++w)
    {
        double pillarEnd;
        if (w < windowCount)
            pillarEnd = wallStart + spacing * (w + 1) + windowWidth * w;
        else
            pillarEnd = (axis == 0) ? wallMaxs.x() : wallMaxs.y();

        // Create the pillar (full height wall segment)
        if (pillarEnd > cursor + 0.01)
        {
            Vector3 pMins = wallMins;
            Vector3 pMaxs = wallMaxs;
            if (axis == 0) { pMins.x() = cursor; pMaxs.x() = pillarEnd; }
            else           { pMins.y() = cursor; pMaxs.y() = pillarEnd; }

            auto node = createBoxBrush(pMins, pMaxs, wallMaterial, parent);
            Node_setSelected(node, true);
        }

        // Create window bay pieces (sill, lintel, and optionally inset)
        if (w < windowCount)
        {
            double winStart = pillarEnd;
            double winEnd = winStart + windowWidth;

            // Sill piece (below window opening)
            if (sillHeight > 0.01)
            {
                Vector3 sMins = wallMins;
                Vector3 sMaxs = wallMaxs;
                if (axis == 0) { sMins.x() = winStart; sMaxs.x() = winEnd; }
                else           { sMins.y() = winStart; sMaxs.y() = winEnd; }
                sMaxs.z() = windowBot;

                auto node = createBoxBrush(sMins, sMaxs, wallMaterial, parent);
                Node_setSelected(node, true);
            }

            // Lintel piece (above window opening)
            if (windowTop < wallMaxs.z() - 0.01)
            {
                Vector3 lMins = wallMins;
                Vector3 lMaxs = wallMaxs;
                if (axis == 0) { lMins.x() = winStart; lMaxs.x() = winEnd; }
                else           { lMins.y() = winStart; lMaxs.y() = winEnd; }
                lMins.z() = windowTop;

                auto node = createBoxBrush(lMins, lMaxs, wallMaterial, parent);
                Node_setSelected(node, true);
            }

            // Window inset (recessed piece at the back of the window opening)
            if (windowInset > 0.01)
            {
                // The wall thickness direction
                double wallDepth = (axis == 0)
                    ? (wallMaxs.y() - wallMins.y())
                    : (wallMaxs.x() - wallMins.x());

                if (windowInset < wallDepth - 0.01)
                {
                    // Inset brush fills the back portion of the window opening
                    Vector3 iMins, iMaxs;
                    iMins.z() = windowBot;
                    iMaxs.z() = windowTop;

                    if (axis == 0)
                    {
                        iMins.x() = winStart;
                        iMaxs.x() = winEnd;
                        // Determine which side is "inside" - use the deeper half
                        double mid = (wallMins.y() + wallMaxs.y()) * 0.5;
                        // For simplicity, place inset on the maxs.y side (interior)
                        iMins.y() = wallMaxs.y() - windowInset;
                        iMaxs.y() = wallMaxs.y();
                    }
                    else
                    {
                        iMins.y() = winStart;
                        iMaxs.y() = winEnd;
                        iMins.x() = wallMaxs.x() - windowInset;
                        iMaxs.x() = wallMaxs.x();
                    }

                    auto node = createBoxBrush(iMins, iMaxs, frameMaterial, parent);
                    Node_setSelected(node, true);
                }
            }

            cursor = winEnd;
        }
    }
}

// Generate a single floor of the building
inline void generateFloor(
    const AABB& footprint, int floorIndex,
    double baseZ, const BuildingParams& params,
    const scene::INodePtr& parent)
{
    double floorBot = baseZ + floorIndex * params.floorHeight;
    double floorTop = floorBot + params.floorHeight;

    double xMin = footprint.origin.x() - footprint.extents.x();
    double xMax = footprint.origin.x() + footprint.extents.x();
    double yMin = footprint.origin.y() - footprint.extents.y();
    double yMax = footprint.origin.y() + footprint.extents.y();
    double wt = params.wallThickness;

    // Floor slab
    {
        Vector3 mins(xMin, yMin, floorBot);
        Vector3 maxs(xMax, yMax, floorBot + params.floorThickness);
        auto node = createBoxBrush(mins, maxs, params.trimMaterial, parent);
        Node_setSelected(node, true);
    }

    // Floor trim (decorative band at the bottom of each floor, on the exterior)
    if (params.trimHeight > 0.01)
    {
        double trimTop = floorBot + params.floorThickness + params.trimHeight;

        // East wall trim (+X side)
        {
            Vector3 mins(xMax - wt, yMin, floorBot + params.floorThickness);
            Vector3 maxs(xMax + 1, yMax, trimTop);
            auto node = createBoxBrush(mins, maxs, params.trimMaterial, parent);
            Node_setSelected(node, true);
        }
        // West wall trim (-X side)
        {
            Vector3 mins(xMin - 1, yMin, floorBot + params.floorThickness);
            Vector3 maxs(xMin + wt, yMax, trimTop);
            auto node = createBoxBrush(mins, maxs, params.trimMaterial, parent);
            Node_setSelected(node, true);
        }
        // North wall trim (+Y side)
        {
            Vector3 mins(xMin, yMax - wt, floorBot + params.floorThickness);
            Vector3 maxs(xMax, yMax + 1, trimTop);
            auto node = createBoxBrush(mins, maxs, params.trimMaterial, parent);
            Node_setSelected(node, true);
        }
        // South wall trim (-Y side)
        {
            Vector3 mins(xMin, yMin - 1, floorBot + params.floorThickness);
            Vector3 maxs(xMax, yMin + wt, trimTop);
            auto node = createBoxBrush(mins, maxs, params.trimMaterial, parent);
            Node_setSelected(node, true);
        }
    }

    // Walls with windows
    double wallBot = floorBot + params.floorThickness;
    double wallTop = floorTop;

    double xLen = xMax - xMin;
    double yLen = yMax - yMin;

    int winCountX, winCountY;
    if (params.windowMode == 0)
    {
        // Automatic
        winCountX = calcAutoWindowCount(xLen, wt, params.windowWidth);
        winCountY = calcAutoWindowCount(yLen, wt, params.windowWidth);
    }
    else
    {
        winCountX = params.windowsPerWall;
        winCountY = params.windowsPerWall;
    }

    double sillH = params.windowSillHeight;
    double winH = params.windowHeight;
    double winW = params.windowWidth;
    double winInset = params.windowInset;

    // East wall (+X face)
    {
        Vector3 wMins(xMax - wt, yMin, wallBot);
        Vector3 wMaxs(xMax, yMax, wallTop);
        generateWallWithWindows(wMins, wMaxs, 1, winCountY,
            winW, winH, sillH, winInset,
            params.wallMaterial, params.windowFrameMaterial, parent);
    }

    // West wall (-X face)
    {
        Vector3 wMins(xMin, yMin, wallBot);
        Vector3 wMaxs(xMin + wt, yMax, wallTop);
        generateWallWithWindows(wMins, wMaxs, 1, winCountY,
            winW, winH, sillH, winInset,
            params.wallMaterial, params.windowFrameMaterial, parent);
    }

    // North wall (+Y face) - exclude corners already covered by E/W walls
    {
        Vector3 wMins(xMin + wt, yMax - wt, wallBot);
        Vector3 wMaxs(xMax - wt, yMax, wallTop);
        generateWallWithWindows(wMins, wMaxs, 0, winCountX,
            winW, winH, sillH, winInset,
            params.wallMaterial, params.windowFrameMaterial, parent);
    }

    // South wall (-Y face)
    {
        Vector3 wMins(xMin + wt, yMin, wallBot);
        Vector3 wMaxs(xMax - wt, yMin + wt, wallTop);
        generateWallWithWindows(wMins, wMaxs, 0, winCountX,
            winW, winH, sillH, winInset,
            params.wallMaterial, params.windowFrameMaterial, parent);
    }
}

// Generate the roof
inline void generateRoof(
    const AABB& footprint, double roofBaseZ,
    const BuildingParams& params, const scene::INodePtr& parent)
{
    double xMin = footprint.origin.x() - footprint.extents.x();
    double xMax = footprint.origin.x() + footprint.extents.x();
    double yMin = footprint.origin.y() - footprint.extents.y();
    double yMax = footprint.origin.y() + footprint.extents.y();
    double ft = params.floorThickness;

    switch (params.roofType)
    {
    case 0: // Flat
    {
        Vector3 mins(xMin, yMin, roofBaseZ);
        Vector3 maxs(xMax, yMax, roofBaseZ + ft);
        auto node = createBoxBrush(mins, maxs, params.trimMaterial, parent);
        Node_setSelected(node, true);
        break;
    }
    case 1: // Flat with border trim
    {
        // Flat roof slab
        Vector3 mins(xMin, yMin, roofBaseZ);
        Vector3 maxs(xMax, yMax, roofBaseZ + ft);
        auto node = createBoxBrush(mins, maxs, params.trimMaterial, parent);
        Node_setSelected(node, true);

        double borderTop = roofBaseZ + ft + params.roofBorderHeight;
        double bw = params.wallThickness;

        // Border walls (parapet)
        // East
        {
            Vector3 bMins(xMax - bw, yMin, roofBaseZ + ft);
            Vector3 bMaxs(xMax, yMax, borderTop);
            auto n = createBoxBrush(bMins, bMaxs, params.trimMaterial, parent);
            Node_setSelected(n, true);
        }
        // West
        {
            Vector3 bMins(xMin, yMin, roofBaseZ + ft);
            Vector3 bMaxs(xMin + bw, yMax, borderTop);
            auto n = createBoxBrush(bMins, bMaxs, params.trimMaterial, parent);
            Node_setSelected(n, true);
        }
        // North
        {
            Vector3 bMins(xMin + bw, yMax - bw, roofBaseZ + ft);
            Vector3 bMaxs(xMax - bw, yMax, borderTop);
            auto n = createBoxBrush(bMins, bMaxs, params.trimMaterial, parent);
            Node_setSelected(n, true);
        }
        // South
        {
            Vector3 bMins(xMin + bw, yMin, roofBaseZ + ft);
            Vector3 bMaxs(xMax - bw, yMin + bw, borderTop);
            auto n = createBoxBrush(bMins, bMaxs, params.trimMaterial, parent);
            Node_setSelected(n, true);
        }
        break;
    }
    case 2: // Slanted
    {
        // A single wedge brush that slopes from one side to the other
        double slopeH = params.roofSlopeHeight;
        int dir = params.roofSlopeDirection;

        // The slope plane normal and distance depend on direction
        // dir: 0=East(slopes up toward +X), 1=North(+Y), 2=West(-X), 3=South(-Y)
        double dx = 0, dy = 0;
        double runLength = 0;
        switch (dir)
        {
        case 0: dx = 1; runLength = xMax - xMin; break;
        case 1: dy = 1; runLength = yMax - yMin; break;
        case 2: dx = -1; runLength = xMax - xMin; break;
        case 3: dy = -1; runLength = yMax - yMin; break;
        }

        // The top surface is a tilted plane.
        // Low edge is at roofBaseZ, high edge is at roofBaseZ + slopeH.
        // Normal of the sloped plane points upward and toward the low side.
        double rise = slopeH;
        double run = runLength;
        double len = std::sqrt(rise * rise + run * run);
        double nx = -dx * rise / len;
        double ny = -dy * rise / len;
        double nz = run / len;

        // A point on the high edge
        double px, py, pz;
        pz = roofBaseZ + slopeH;
        switch (dir)
        {
        case 0: px = xMax; py = (yMin + yMax) * 0.5; break;
        case 1: px = (xMin + xMax) * 0.5; py = yMax; break;
        case 2: px = xMin; py = (yMin + yMax) * 0.5; break;
        case 3: px = (xMin + xMax) * 0.5; py = yMin; break;
        }
        double dist = nx * px + ny * py + nz * pz;

        std::vector<Plane3> faces;
        // Sloped top
        faces.push_back(Plane3(nx, ny, nz, dist));
        // Bottom
        faces.push_back(Plane3(0, 0, -1, -roofBaseZ));
        // Sides
        faces.push_back(Plane3( 1, 0, 0, xMax));
        faces.push_back(Plane3(-1, 0, 0, -xMin));
        faces.push_back(Plane3( 0, 1, 0, yMax));
        faces.push_back(Plane3( 0,-1, 0, -yMin));

        auto node = createWedgeBrush(faces, params.trimMaterial, parent);
        Node_setSelected(node, true);
        break;
    }
    case 3: // A-Frame
    {
        double peakH = params.aRoofHeight;
        int ridgeDir = params.aRoofDirection; // 0=E-W ridge, 1=N-S ridge

        if (ridgeDir == 0)
        {
            // Ridge runs along X axis, slopes on North and South sides
            double midY = (yMin + yMax) * 0.5;
            double halfSpan = (yMax - yMin) * 0.5;
            double rise = peakH;
            double len = std::sqrt(rise * rise + halfSpan * halfSpan);

            // South slope (from yMin up to ridge)
            {
                double ny = -rise / len;
                double nz = halfSpan / len;
                double dist = ny * midY + nz * (roofBaseZ + peakH);

                std::vector<Plane3> faces;
                faces.push_back(Plane3(0, ny, nz, dist));     // sloped top
                faces.push_back(Plane3(0, 0, -1, -roofBaseZ)); // bottom
                faces.push_back(Plane3( 1, 0, 0, xMax));
                faces.push_back(Plane3(-1, 0, 0, -xMin));
                faces.push_back(Plane3( 0, -1, 0, -yMin));     // south face
                faces.push_back(Plane3( 0, 1, 0, midY));       // center clip

                auto node = createWedgeBrush(faces, params.trimMaterial, parent);
                Node_setSelected(node, true);
            }
            // North slope (from yMax up to ridge)
            {
                double ny = rise / len;
                double nz = halfSpan / len;
                double dist = ny * midY + nz * (roofBaseZ + peakH);

                std::vector<Plane3> faces;
                faces.push_back(Plane3(0, ny, nz, dist));
                faces.push_back(Plane3(0, 0, -1, -roofBaseZ));
                faces.push_back(Plane3( 1, 0, 0, xMax));
                faces.push_back(Plane3(-1, 0, 0, -xMin));
                faces.push_back(Plane3( 0, 1, 0, yMax));
                faces.push_back(Plane3( 0, -1, 0, -midY));

                auto node = createWedgeBrush(faces, params.trimMaterial, parent);
                Node_setSelected(node, true);
            }
        }
        else
        {
            // Ridge runs along Y axis, slopes on East and West sides
            double midX = (xMin + xMax) * 0.5;
            double halfSpan = (xMax - xMin) * 0.5;
            double rise = peakH;
            double len = std::sqrt(rise * rise + halfSpan * halfSpan);

            // West slope
            {
                double nx = -rise / len;
                double nz = halfSpan / len;
                double dist = nx * midX + nz * (roofBaseZ + peakH);

                std::vector<Plane3> faces;
                faces.push_back(Plane3(nx, 0, nz, dist));
                faces.push_back(Plane3(0, 0, -1, -roofBaseZ));
                faces.push_back(Plane3(-1, 0, 0, -xMin));
                faces.push_back(Plane3( 1, 0, 0, midX));
                faces.push_back(Plane3( 0, 1, 0, yMax));
                faces.push_back(Plane3( 0, -1, 0, -yMin));

                auto node = createWedgeBrush(faces, params.trimMaterial, parent);
                Node_setSelected(node, true);
            }
            // East slope
            {
                double nx = rise / len;
                double nz = halfSpan / len;
                double dist = nx * midX + nz * (roofBaseZ + peakH);

                std::vector<Plane3> faces;
                faces.push_back(Plane3(nx, 0, nz, dist));
                faces.push_back(Plane3(0, 0, -1, -roofBaseZ));
                faces.push_back(Plane3( 1, 0, 0, xMax));
                faces.push_back(Plane3(-1, 0, 0, -midX));
                faces.push_back(Plane3( 0, 1, 0, yMax));
                faces.push_back(Plane3( 0, -1, 0, -yMin));

                auto node = createWedgeBrush(faces, params.trimMaterial, parent);
                Node_setSelected(node, true);
            }
        }
        break;
    }
    }
}

// Main entry: generate the full building from the selected brush's AABB
inline void generateBuilding(
    const AABB& footprint, const BuildingParams& params,
    const scene::INodePtr& parent)
{
    double baseZ = footprint.origin.z() - footprint.extents.z();
    int floorCount = std::max(1, params.floorCount);

    for (int i = 0; i < floorCount; ++i)
    {
        generateFloor(footprint, i, baseZ, params, parent);
    }

    // Roof at the top of the last floor
    double roofZ = baseZ + floorCount * params.floorHeight;
    generateRoof(footprint, roofZ, params, parent);
}

} // namespace building
