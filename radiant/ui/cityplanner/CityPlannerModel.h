#pragma once

#include "ui/building/BuildingGeometry.h"
#include "ui/tilemap/TileMapGeometry.h"

#include <algorithm>
#include <cstdint>
#include <queue>
#include <string>
#include <utility>
#include <vector>

namespace cityplanner
{

enum class CellType : uint8_t
{
    Empty = 0,
    Floor,
    Wall,
    HalfSquareN,
    HalfSquareS,
    HalfSquareE,
    HalfSquareW,
    Building,
    WindowlessBuilding
};

inline bool isHalfSquareType(CellType t)
{
    return t == CellType::HalfSquareN || t == CellType::HalfSquareS ||
           t == CellType::HalfSquareE || t == CellType::HalfSquareW;
}

struct CityCell
{
    CellType type = CellType::Empty;
    std::string material;
    int buildingGroupId = -1;
};

struct BuildingGroup
{
    CellType type = CellType::Building;
    building::BuildingParams params;
    std::vector<std::pair<int, int>> cells;
    int x0 = 0, y0 = 0, x1 = 0, y1 = 0;
};

inline std::vector<tilemap::Rect> mergeCellRects(
    const std::vector<std::vector<CityCell>>& grid,
    int gridW, int gridH,
    CellType matchType)
{
    std::vector<std::vector<bool>> visited(gridH, std::vector<bool>(gridW, false));
    std::vector<tilemap::Rect> rects;

    for (int y = 0; y < gridH; ++y)
    {
        if (y >= static_cast<int>(grid.size())) break;

        for (int x = 0; x < gridW; ++x)
        {
            if (x >= static_cast<int>(grid[y].size())) break;
            if (visited[y][x] || grid[y][x].type != matchType)
                continue;

            const std::string mat = grid[y][x].material;

            int maxX = x;
            while (maxX + 1 < gridW &&
                   maxX + 1 < static_cast<int>(grid[y].size()) &&
                   !visited[y][maxX + 1] &&
                   grid[y][maxX + 1].type == matchType &&
                   grid[y][maxX + 1].material == mat)
                ++maxX;

            int maxY = y;
            bool canExpand = true;
            while (canExpand && maxY + 1 < gridH &&
                   maxY + 1 < static_cast<int>(grid.size()))
            {
                for (int cx = x; cx <= maxX; ++cx)
                {
                    if (cx >= static_cast<int>(grid[maxY + 1].size()) ||
                        visited[maxY + 1][cx] ||
                        grid[maxY + 1][cx].type != matchType ||
                        grid[maxY + 1][cx].material != mat)
                    {
                        canExpand = false;
                        break;
                    }
                }
                if (canExpand)
                    ++maxY;
            }

            tilemap::Rect r{x, y, maxX, maxY};
            for (int fy = r.y0; fy <= r.y1; ++fy)
                for (int fx = r.x0; fx <= r.x1; ++fx)
                    visited[fy][fx] = true;

            rects.push_back(r);
        }
    }

    return rects;
}

inline bool isBuildingType(CellType t)
{
    return t == CellType::Building || t == CellType::WindowlessBuilding;
}

inline std::vector<BuildingGroup> computeBuildingGroups(
    std::vector<std::vector<CityCell>>& grid,
    int gridW, int gridH,
    const std::vector<BuildingGroup>& existingGroups,
    const building::BuildingParams& defaultParams)
{
    std::vector<BuildingGroup> result;

    std::vector<std::vector<int>> oldIds(gridH, std::vector<int>(gridW, -1));
    for (int y = 0; y < gridH && y < static_cast<int>(grid.size()); ++y)
    {
        for (int x = 0; x < gridW && x < static_cast<int>(grid[y].size()); ++x)
        {
            if (isBuildingType(grid[y][x].type))
            {
                oldIds[y][x] = grid[y][x].buildingGroupId;
                grid[y][x].buildingGroupId = -1;
            }
        }
    }

    for (int y = 0; y < gridH && y < static_cast<int>(grid.size()); ++y)
    {
        for (int x = 0; x < gridW && x < static_cast<int>(grid[y].size()); ++x)
        {
            if (!isBuildingType(grid[y][x].type) ||
                grid[y][x].buildingGroupId != -1)
                continue;

            CellType seedType = grid[y][x].type;

            BuildingGroup g;
            g.type = seedType;
            g.params = defaultParams;
            g.x0 = g.x1 = x;
            g.y0 = g.y1 = y;
            int newId = static_cast<int>(result.size());
            int inheritFrom = oldIds[y][x];

            std::queue<std::pair<int, int>> q;
            q.push({x, y});
            grid[y][x].buildingGroupId = newId;

            while (!q.empty())
            {
                auto cell = q.front();
                q.pop();
                int cc = cell.first, rr = cell.second;

                g.cells.emplace_back(cc, rr);
                g.x0 = std::min(g.x0, cc);
                g.x1 = std::max(g.x1, cc);
                g.y0 = std::min(g.y0, rr);
                g.y1 = std::max(g.y1, rr);

                if (inheritFrom < 0 && oldIds[rr][cc] >= 0)
                    inheritFrom = oldIds[rr][cc];

                static const int dc[] = {1, -1, 0, 0};
                static const int dr[] = {0, 0, 1, -1};
                for (int k = 0; k < 4; ++k)
                {
                    int nc = cc + dc[k], nr = rr + dr[k];
                    if (nc < 0 || nc >= gridW || nr < 0 || nr >= gridH)
                        continue;
                    if (nr >= static_cast<int>(grid.size()) ||
                        nc >= static_cast<int>(grid[nr].size()))
                        continue;
                    if (grid[nr][nc].type != seedType ||
                        grid[nr][nc].buildingGroupId != -1)
                        continue;
                    grid[nr][nc].buildingGroupId = newId;
                    q.push({nc, nr});
                }
            }

            if (inheritFrom >= 0 && inheritFrom < static_cast<int>(existingGroups.size()))
                g.params = existingGroups[inheritFrom].params;

            result.push_back(std::move(g));
        }
    }

    return result;
}

inline std::vector<tilemap::Rect> mergeRowRuns(
    const std::vector<std::vector<CityCell>>& grid,
    int gridW, int gridH,
    CellType matchType)
{
    std::vector<tilemap::Rect> runs;

    for (int y = 0; y < gridH && y < static_cast<int>(grid.size()); ++y)
    {
        int x = 0;
        while (x < gridW && x < static_cast<int>(grid[y].size()))
        {
            if (grid[y][x].type != matchType) { ++x; continue; }
            const std::string mat = grid[y][x].material;
            int start = x;
            while (x < gridW && x < static_cast<int>(grid[y].size()) &&
                   grid[y][x].type == matchType &&
                   grid[y][x].material == mat) ++x;
            runs.push_back({start, y, x - 1, y});
        }
    }

    return runs;
}

inline std::vector<tilemap::Rect> mergeColumnRuns(
    const std::vector<std::vector<CityCell>>& grid,
    int gridW, int gridH,
    CellType matchType)
{
    std::vector<tilemap::Rect> runs;

    for (int x = 0; x < gridW; ++x)
    {
        int y = 0;
        while (y < gridH && y < static_cast<int>(grid.size()))
        {
            if (x >= static_cast<int>(grid[y].size()) || grid[y][x].type != matchType)
            { ++y; continue; }
            const std::string mat = grid[y][x].material;
            int start = y;
            while (y < gridH && y < static_cast<int>(grid.size()) &&
                   x < static_cast<int>(grid[y].size()) &&
                   grid[y][x].type == matchType &&
                   grid[y][x].material == mat) ++y;
            runs.push_back({x, start, x, y - 1});
        }
    }

    return runs;
}

} // namespace cityplanner
