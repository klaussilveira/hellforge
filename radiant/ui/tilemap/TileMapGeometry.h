#pragma once

#include "ibrush.h"
#include "ipatch.h"
#include "scenelib.h"
#include "math/Plane3.h"
#include "math/Matrix3.h"
#include "math/Vector3.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

namespace tilemap
{

enum class TileType : uint8_t
{
    Empty = 0,
    Room,
    SlopeNW,
    SlopeNE,
    SlopeSW,
    SlopeSE,
    CurveNW,
    CurveNE,
    CurveSW,
    CurveSE
};

inline bool isSlopeType(TileType t)
{
    return t == TileType::SlopeNW || t == TileType::SlopeNE ||
           t == TileType::SlopeSW || t == TileType::SlopeSE;
}

inline bool isCurveType(TileType t)
{
    return t == TileType::CurveNW || t == TileType::CurveNE ||
           t == TileType::CurveSW || t == TileType::CurveSE;
}

inline bool isSolidType(TileType t)
{
    return t != TileType::Empty;
}

struct TileMaterials
{
    std::string floor;
    std::string ceiling;
    std::string wallNorth;
    std::string wallSouth;
    std::string wallEast;
    std::string wallWest;
};

struct StairsParams
{
    int stepCount = 8;
    float stepHeight = 16.0f;
    bool solid = true;
    int direction = 0;
    std::string material;
};

struct LightParams
{
    float radius = 128.0f;
    Vector3 color = Vector3(1, 1, 1);
    std::string texture;
};

struct Tile
{
    TileType type = TileType::Empty;
    TileMaterials materials;
    bool hasStairs = false;
    StairsParams stairsParams;
    bool hasLight = false;
    LightParams lightParams;
};

struct Rect
{
    int x0, y0, x1, y1;
};

inline scene::INodePtr createBoxBrushMultiMat(
    const Vector3& mins, const Vector3& maxs,
    const std::string& matPosX, const std::string& matNegX,
    const std::string& matPosY, const std::string& matNegY,
    const std::string& matPosZ, const std::string& matNegZ,
    const scene::INodePtr& parent)
{
    auto brushNode = GlobalBrushCreator().createBrush();
    parent->addChildNode(brushNode);

    auto& brush = *Node_getIBrush(brushNode);

    double texScale = 0.0078125;
    Matrix3 proj = Matrix3::getIdentity();
    proj.xx() = texScale;
    proj.yy() = texScale;

    brush.addFace(Plane3( 1, 0, 0,  maxs.x()), proj, matPosX);
    brush.addFace(Plane3(-1, 0, 0, -mins.x()), proj, matNegX);
    brush.addFace(Plane3( 0, 1, 0,  maxs.y()), proj, matPosY);
    brush.addFace(Plane3( 0,-1, 0, -mins.y()), proj, matNegY);
    brush.addFace(Plane3( 0, 0, 1,  maxs.z()), proj, matPosZ);
    brush.addFace(Plane3( 0, 0,-1, -mins.z()), proj, matNegZ);

    brush.evaluateBRep();
    return brushNode;
}

inline scene::INodePtr createBoxBrush(
    const Vector3& mins, const Vector3& maxs,
    const std::string& material, const scene::INodePtr& parent)
{
    return createBoxBrushMultiMat(mins, maxs,
        material, material, material, material, material, material, parent);
}


inline bool allTilesShareMaterials(
    const std::vector<std::vector<Tile>>& grid,
    const Rect& rect)
{
    const auto& ref = grid[rect.y0][rect.x0].materials;
    for (int y = rect.y0; y <= rect.y1; ++y)
        for (int x = rect.x0; x <= rect.x1; ++x)
        {
            const auto& m = grid[y][x].materials;
            if (m.floor != ref.floor || m.ceiling != ref.ceiling ||
                m.wallNorth != ref.wallNorth || m.wallSouth != ref.wallSouth ||
                m.wallEast != ref.wallEast || m.wallWest != ref.wallWest)
                return false;
        }
    return true;
}

inline std::vector<Rect> mergeRoomTiles(
    const std::vector<std::vector<Tile>>& grid,
    int gridW, int gridH)
{
    std::vector<std::vector<bool>> visited(gridH, std::vector<bool>(gridW, false));
    std::vector<Rect> rects;

    for (int y = 0; y < gridH; ++y)
    {
        for (int x = 0; x < gridW; ++x)
        {
            if (visited[y][x] || !isSolidType(grid[y][x].type))
                continue;

            int maxX = x;
            while (maxX + 1 < gridW && !visited[y][maxX + 1] &&
                   isSolidType(grid[y][maxX + 1].type))
                ++maxX;

            int maxY = y;
            bool canExpand = true;
            while (canExpand && maxY + 1 < gridH)
            {
                for (int cx = x; cx <= maxX; ++cx)
                {
                    if (visited[maxY + 1][cx] || !isSolidType(grid[maxY + 1][cx].type))
                    {
                        canExpand = false;
                        break;
                    }
                }
                if (canExpand)
                    ++maxY;
            }

            Rect r{x, y, maxX, maxY};
            if (!allTilesShareMaterials(grid, r))
                r = {x, y, x, y};

            for (int fy = r.y0; fy <= r.y1; ++fy)
                for (int fx = r.x0; fx <= r.x1; ++fx)
                    visited[fy][fx] = true;

            rects.push_back(r);
        }
    }

    return rects;
}

inline void generateRoomBrushes(
    const Vector3& origin,
    const std::vector<std::vector<Tile>>& grid,
    int gridW, int gridH,
    float tileW, float tileH,
    float floorZ, float ceilHeight,
    const scene::INodePtr& parent)
{
    auto rects = mergeRoomTiles(grid, gridW, gridH);

    for (auto& rect : rects)
    {
        const auto& mats = grid[rect.y0][rect.x0].materials;

        float x0 = origin.x() + rect.x0 * tileW;
        float y0 = origin.y() + rect.y0 * tileH;
        float x1 = origin.x() + (rect.x1 + 1) * tileW;
        float y1 = origin.y() + (rect.y1 + 1) * tileH;

        auto brushNode = GlobalBrushCreator().createBrush();
        parent->addChildNode(brushNode);

        auto& brush = *Node_getIBrush(brushNode);

        double texScale = 0.0078125;
        Matrix3 proj = Matrix3::getIdentity();
        proj.xx() = texScale;
        proj.yy() = texScale;

        brush.addFace(Plane3( 1, 0, 0,  x1), proj, mats.wallEast);
        brush.addFace(Plane3(-1, 0, 0, -x0), proj, mats.wallWest);
        brush.addFace(Plane3( 0, 1, 0,  y1), proj, mats.wallNorth);
        brush.addFace(Plane3( 0,-1, 0, -y0), proj, mats.wallSouth);
        brush.addFace(Plane3( 0, 0, 1,  floorZ + ceilHeight), proj, mats.ceiling);
        brush.addFace(Plane3( 0, 0,-1, -floorZ), proj, mats.floor);

        for (int gy = rect.y0; gy <= rect.y1; ++gy)
        {
            for (int gx = rect.x0; gx <= rect.x1; ++gx)
            {
                TileType t = grid[gy][gx].type;
                if (!isSlopeType(t))
                    continue;

                double cx0 = origin.x() + gx * tileW;
                double cy0 = origin.y() + gy * tileH;
                double cx1 = cx0 + tileW;
                double tw = tileW, th = tileH;
                double L = std::sqrt(tw * tw + th * th);

                switch (t)
                {
                case TileType::SlopeNW:
                    brush.addFace(Plane3(th/L, tw/L, 0, (th*cx1 + tw*cy0)/L), proj, mats.wallNorth);
                    break;
                case TileType::SlopeNE:
                    brush.addFace(Plane3(-th/L, tw/L, 0, (-th*cx0 + tw*cy0)/L), proj, mats.wallNorth);
                    break;
                case TileType::SlopeSW:
                    brush.addFace(Plane3(th/L, -tw/L, 0, (th*cx0 - tw*cy0)/L), proj, mats.wallSouth);
                    break;
                case TileType::SlopeSE:
                    brush.addFace(Plane3(-th/L, -tw/L, 0, (-th*cx1 - tw*cy0)/L), proj, mats.wallSouth);
                    break;
                default:
                    break;
                }
            }
        }

        brush.evaluateBRep();
        Node_setSelected(brushNode, true);
    }
}

inline void generateStairsBrushesForTile(
    const Vector3& tileOrigin, float tileW, float tileH, float floorZ,
    const StairsParams& params, const scene::INodePtr& parent)
{
    double dirDeg = params.direction * 90.0;
    double rad = dirDeg * (3.14159265358979323846 / 180.0);
    double dx = std::cos(rad);
    double dy = std::sin(rad);

    float totalDepth = (params.direction % 2 == 0) ? tileW : tileH;
    float stepDepth = totalDepth / params.stepCount;
    float width = (params.direction % 2 == 0) ? tileH : tileW;

    float cx = tileOrigin.x() + tileW * 0.5f;
    float cy = tileOrigin.y() + tileH * 0.5f;
    float startX = cx - dx * totalDepth * 0.5f;
    float startY = cy - dy * totalDepth * 0.5f;

    double px = -dy;
    double py = dx;

    for (int i = 0; i < params.stepCount; ++i)
    {
        double d0 = i * stepDepth;
        double d1 = (i + 1) * stepDepth;
        double zBot = params.solid ? floorZ : floorZ + i * params.stepHeight;
        double zTop = floorZ + (i + 1) * params.stepHeight;

        double ax = startX + dx * d0;
        double ay = startY + dy * d0;
        double bx = startX + dx * d1;
        double by = startY + dy * d1;

        double c0x = ax - px * width * 0.5;
        double c0y = ay - py * width * 0.5;
        double c1x = bx - px * width * 0.5;
        double c1y = by - py * width * 0.5;
        double c2x = ax + px * width * 0.5;
        double c2y = ay + py * width * 0.5;
        double c3x = bx + px * width * 0.5;
        double c3y = by + py * width * 0.5;

        Vector3 mins(
            std::min({c0x, c1x, c2x, c3x}),
            std::min({c0y, c1y, c2y, c3y}),
            zBot);
        Vector3 maxs(
            std::max({c0x, c1x, c2x, c3x}),
            std::max({c0y, c1y, c2y, c3y}),
            zTop);

        auto node = createBoxBrush(mins, maxs, params.material, parent);
        Node_setSelected(node, true);
    }
}

inline void generateAllStairs(
    const Vector3& origin,
    const std::vector<std::vector<Tile>>& grid,
    int gridW, int gridH,
    float tileW, float tileH,
    float floorZ, const scene::INodePtr& parent)
{
    for (int y = 0; y < gridH; ++y)
    {
        for (int x = 0; x < gridW; ++x)
        {
            if (!grid[y][x].hasStairs)
                continue;

            Vector3 tileOrigin(
                origin.x() + x * tileW,
                origin.y() + y * tileH,
                0);

            generateStairsBrushesForTile(tileOrigin, tileW, tileH, floorZ,
                grid[y][x].stairsParams, parent);
        }
    }
}

inline void generateCurvePatchForTile(
    const Vector3& tileOrigin, float tileW, float tileH,
    float floorZ, float ceilHeight, TileType type,
    const std::string& material, const scene::INodePtr& parent)
{
    const double PI_HALF = 1.5707963267948966;
    const int SEGMENTS = 3;
    const int patchWidth = 2 * SEGMENTS + 1;
    const int patchHeight = 3;

    double cx0 = tileOrigin.x();
    double cy0 = tileOrigin.y();
    double cx1 = cx0 + tileW;
    double cy1 = cy0 + tileH;
    double r1 = tileW, r2 = tileH;

    Vector3 center;
    Vector3 d1, d2;

    switch (type)
    {
    case TileType::CurveNW:
        center = Vector3(cx0, cy0, 0);
        d1 = Vector3(0, 1, 0);
        d2 = Vector3(1, 0, 0);
        r1 = tileH; r2 = tileW;
        break;
    case TileType::CurveNE:
        center = Vector3(cx1, cy0, 0);
        d1 = Vector3(-1, 0, 0);
        d2 = Vector3(0, 1, 0);
        break;
    case TileType::CurveSW:
        center = Vector3(cx0, cy1, 0);
        d1 = Vector3(1, 0, 0);
        d2 = Vector3(0, -1, 0);
        break;
    case TileType::CurveSE:
        center = Vector3(cx1, cy1, 0);
        d1 = Vector3(0, -1, 0);
        d2 = Vector3(-1, 0, 0);
        r1 = tileH; r2 = tileW;
        break;
    default:
        return;
    }

    auto patchNode = GlobalPatchModule().createPatch(patch::PatchDefType::Def2);
    parent->addChildNode(patchNode);

    auto* patch = Node_getIPatch(patchNode);
    patch->setDims(static_cast<std::size_t>(patchWidth),
                   static_cast<std::size_t>(patchHeight));
    patch->setShader(material);

    double anglePerSeg = PI_HALF / SEGMENTS;

    for (int row = 0; row < patchHeight; ++row)
    {
        double z = floorZ + (ceilHeight * row) / (patchHeight - 1);

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

            pos.z() = z;
            patch->ctrlAt(row, col).vertex = pos;
        }
    }

    patch->controlPointsChanged();
    patch->scaleTextureNaturally();
    Node_setSelected(patchNode, true);
}

inline void generateAllCurvePatches(
    const Vector3& origin,
    const std::vector<std::vector<Tile>>& grid,
    int gridW, int gridH,
    float tileW, float tileH,
    float floorZ, float ceilHeight,
    const scene::INodePtr& parent)
{
    for (int y = 0; y < gridH; ++y)
    {
        for (int x = 0; x < gridW; ++x)
        {
            if (!isCurveType(grid[y][x].type))
                continue;

            Vector3 tileOrigin(
                origin.x() + x * tileW,
                origin.y() + y * tileH,
                0);

            std::string mat = grid[y][x].materials.wallNorth;

            generateCurvePatchForTile(tileOrigin, tileW, tileH,
                floorZ, ceilHeight, grid[y][x].type,
                mat, parent);
        }
    }
}

void generateAllLights(
    const Vector3& origin,
    const std::vector<std::vector<Tile>>& grid,
    int gridW, int gridH,
    float tileW, float tileH,
    float floorZ, float ceilHeight);

} // namespace tilemap
