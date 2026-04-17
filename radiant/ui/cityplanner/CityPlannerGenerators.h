#pragma once

#include "CityPlannerModel.h"

#include <algorithm>
#include <cstdint>
#include <random>
#include <string>
#include <vector>

namespace cityplanner
{

struct GeneratorMaterials
{
    std::string floor;
    std::string wall;
};

inline void clearGrid(std::vector<std::vector<CityCell>>& grid, int cols, int rows)
{
    for (int r = 0; r < rows && r < static_cast<int>(grid.size()); ++r)
        for (int c = 0; c < cols && c < static_cast<int>(grid[r].size()); ++c)
            grid[r][c] = CityCell();
}

// Grid City: regular street grid. Each block becomes one connected Building
// (which the city planner will turn into a single rect building per group).
// Some blocks are parks (Floor) or windowless variants for visual variety.
inline void generateGridCity(
    std::vector<std::vector<CityCell>>& grid,
    int cols, int rows,
    const GeneratorMaterials& mats,
    uint32_t seed)
{
    std::mt19937 rng(seed);
    std::uniform_real_distribution<double> prob(0.0, 1.0);

    clearGrid(grid, cols, rows);

    const int blockSize = 5;
    const int streetWidth = 1;
    const int period = blockSize + streetWidth;

    int numBlocksR = (rows + period - 1) / period;
    int numBlocksC = (cols + period - 1) / period;

    enum BlockKind : int { Park = 0, Windowed = 1, Windowless = 2 };
    std::vector<std::vector<int>> blockKind(numBlocksR, std::vector<int>(numBlocksC, Windowed));
    for (int br = 0; br < numBlocksR; ++br)
    {
        for (int bc = 0; bc < numBlocksC; ++bc)
        {
            double p = prob(rng);
            if (p < 0.10)      blockKind[br][bc] = Park;
            else if (p < 0.30) blockKind[br][bc] = Windowless;
            else               blockKind[br][bc] = Windowed;
        }
    }

    for (int r = 0; r < rows && r < static_cast<int>(grid.size()); ++r)
    {
        for (int c = 0; c < cols && c < static_cast<int>(grid[r].size()); ++c)
        {
            bool street = (r % period < streetWidth) || (c % period < streetWidth);
            if (street)
            {
                grid[r][c].type = CellType::Floor;
                grid[r][c].material = mats.floor;
                continue;
            }

            int kind = blockKind[r / period][c / period];
            if (kind == Park)
            {
                grid[r][c].type = CellType::Floor;
                grid[r][c].material = mats.floor;
            }
            else if (kind == Windowless)
            {
                grid[r][c].type = CellType::WindowlessBuilding;
            }
            else
            {
                grid[r][c].type = CellType::Building;
            }
        }
    }
}

// Medieval Village: central Floor plaza, scattered Building clusters of varying
// sizes (some windowless = stables/storage), Floor paths between them, and
// HalfSquare fence fragments at building edges. Connected adjacent Buildings
// merge into L-shaped or T-shaped houses, which suits the grid aesthetic.
inline void generateMedievalVillage(
    std::vector<std::vector<CityCell>>& grid,
    int cols, int rows,
    const GeneratorMaterials& mats,
    uint32_t seed)
{
    std::mt19937 rng(seed);
    std::uniform_real_distribution<double> prob(0.0, 1.0);

    clearGrid(grid, cols, rows);

    int cx = cols / 2;
    int cy = rows / 2;
    int plazaR = std::max(1, std::min(cols, rows) / 6);
    for (int r = std::max(0, cy - plazaR); r <= std::min(rows - 1, cy + plazaR); ++r)
    {
        if (r >= static_cast<int>(grid.size())) break;
        for (int c = std::max(0, cx - plazaR); c <= std::min(cols - 1, cx + plazaR); ++c)
        {
            if (c >= static_cast<int>(grid[r].size())) break;
            grid[r][c].type = CellType::Floor;
            grid[r][c].material = mats.floor;
        }
    }

    int target = std::max(4, (cols * rows) / 14);
    std::uniform_int_distribution<int> xDist(0, cols - 1);
    std::uniform_int_distribution<int> yDist(0, rows - 1);
    std::uniform_int_distribution<int> sizeDist(1, 3);

    auto isCellEmpty = [&](int c, int r) {
        if (c < 0 || c >= cols || r < 0 || r >= rows) return false;
        if (r >= static_cast<int>(grid.size())) return false;
        if (c >= static_cast<int>(grid[r].size())) return false;
        return grid[r][c].type == CellType::Empty;
    };

    int placed = 0;
    for (int attempt = 0; attempt < target * 8 && placed < target; ++attempt)
    {
        int w = sizeDist(rng);
        int h = sizeDist(rng);
        int x = xDist(rng);
        int y = yDist(rng);

        bool ok = true;
        for (int r = y; r < y + h && ok; ++r)
            for (int c = x; c < x + w && ok; ++c)
                if (!isCellEmpty(c, r)) ok = false;
        if (!ok) continue;

        CellType bt = (prob(rng) < 0.20)
            ? CellType::WindowlessBuilding
            : CellType::Building;
        for (int r = y; r < y + h; ++r)
            for (int c = x; c < x + w; ++c)
                grid[r][c].type = bt;
        ++placed;
    }

    for (int r = 0; r < rows && r < static_cast<int>(grid.size()); ++r)
    {
        for (int c = 0; c < cols && c < static_cast<int>(grid[r].size()); ++c)
        {
            if (grid[r][c].type == CellType::Empty)
            {
                grid[r][c].type = CellType::Floor;
                grid[r][c].material = mats.floor;
            }
        }
    }

    static const int dc[] = {0, 0, 1, -1};
    static const int dr[] = {1, -1, 0, 0};
    static const CellType halfFor[] = {
        CellType::HalfSquareN,
        CellType::HalfSquareS,
        CellType::HalfSquareE,
        CellType::HalfSquareW
    };

    for (int r = 0; r < rows && r < static_cast<int>(grid.size()); ++r)
    {
        for (int c = 0; c < cols && c < static_cast<int>(grid[r].size()); ++c)
        {
            if (grid[r][c].type != CellType::Floor) continue;
            if (prob(rng) > 0.18) continue;

            for (int k = 0; k < 4; ++k)
            {
                int nr = r + dr[k], nc = c + dc[k];
                if (nr < 0 || nr >= rows || nc < 0 || nc >= cols) continue;
                if (nr >= static_cast<int>(grid.size())) continue;
                if (nc >= static_cast<int>(grid[nr].size())) continue;
                CellType nt = grid[nr][nc].type;
                if (nt == CellType::Building || nt == CellType::WindowlessBuilding)
                {
                    grid[r][c].type = halfFor[k];
                    grid[r][c].material = mats.wall;
                    break;
                }
            }
        }
    }
}

// Downtown: tight street grid with 3-cell blocks, no parks, ~35% windowless
// mix. Uses the same grid structure as Grid City but denser.
inline void generateDowntown(
    std::vector<std::vector<CityCell>>& grid,
    int cols, int rows,
    const GeneratorMaterials& mats,
    uint32_t seed)
{
    std::mt19937 rng(seed);
    std::uniform_real_distribution<double> prob(0.0, 1.0);

    clearGrid(grid, cols, rows);

    const int blockSize = 3;
    const int streetWidth = 1;
    const int period = blockSize + streetWidth;

    int numBlocksR = (rows + period - 1) / period;
    int numBlocksC = (cols + period - 1) / period;

    std::vector<std::vector<int>> blockKind(numBlocksR, std::vector<int>(numBlocksC, 0));
    for (int br = 0; br < numBlocksR; ++br)
        for (int bc = 0; bc < numBlocksC; ++bc)
            blockKind[br][bc] = (prob(rng) < 0.35) ? 1 : 0;

    for (int r = 0; r < rows && r < static_cast<int>(grid.size()); ++r)
    {
        for (int c = 0; c < cols && c < static_cast<int>(grid[r].size()); ++c)
        {
            bool street = (r % period < streetWidth) || (c % period < streetWidth);
            if (street)
            {
                grid[r][c].type = CellType::Floor;
                grid[r][c].material = mats.floor;
            }
            else
            {
                int kind = blockKind[r / period][c / period];
                grid[r][c].type = (kind == 1)
                    ? CellType::WindowlessBuilding : CellType::Building;
            }
        }
    }
}

// Industrial Park: perimeter Wall with a gate, big Windowless Buildings
// separated by Floor access roads.
inline void generateIndustrialPark(
    std::vector<std::vector<CityCell>>& grid,
    int cols, int rows,
    const GeneratorMaterials& mats,
    uint32_t seed)
{
    std::mt19937 rng(seed);
    std::uniform_real_distribution<double> prob(0.0, 1.0);

    clearGrid(grid, cols, rows);

    for (int r = 0; r < rows && r < static_cast<int>(grid.size()); ++r)
        for (int c = 0; c < cols && c < static_cast<int>(grid[r].size()); ++c)
        {
            grid[r][c].type = CellType::Floor;
            grid[r][c].material = mats.floor;
        }

    int innerX0 = 1, innerY0 = 1;
    int innerX1 = cols - 2, innerY1 = rows - 2;

    int target = std::max(2, (cols * rows) / 55);
    int placed = 0;
    for (int attempt = 0; attempt < target * 25 && placed < target; ++attempt)
    {
        int w = std::uniform_int_distribution<int>(3, 6)(rng);
        int h = std::uniform_int_distribution<int>(3, 6)(rng);
        if (w > innerX1 - innerX0 + 1 || h > innerY1 - innerY0 + 1) continue;

        int x = std::uniform_int_distribution<int>(innerX0, innerX1 - w + 1)(rng);
        int y = std::uniform_int_distribution<int>(innerY0, innerY1 - h + 1)(rng);

        bool clear = true;
        for (int r = y - 1; r <= y + h && clear; ++r)
            for (int c = x - 1; c <= x + w && clear; ++c)
            {
                if (r < 0 || r >= rows || c < 0 || c >= cols) continue;
                CellType t = grid[r][c].type;
                if (t == CellType::Building || t == CellType::WindowlessBuilding)
                    clear = false;
            }
        if (!clear) continue;

        CellType bt = (prob(rng) < 0.85)
            ? CellType::WindowlessBuilding : CellType::Building;
        for (int r = y; r < y + h; ++r)
            for (int c = x; c < x + w; ++c)
                grid[r][c].type = bt;
        ++placed;
    }

    for (int c = 0; c < cols && c < static_cast<int>(grid[0].size()); ++c)
    {
        grid[0][c].type = CellType::Wall; grid[0][c].material = mats.wall;
        if (rows - 1 < static_cast<int>(grid.size()) &&
            c < static_cast<int>(grid[rows - 1].size()))
        {
            grid[rows - 1][c].type = CellType::Wall;
            grid[rows - 1][c].material = mats.wall;
        }
    }
    for (int r = 0; r < rows && r < static_cast<int>(grid.size()); ++r)
    {
        if (!grid[r].empty())
        {
            grid[r][0].type = CellType::Wall; grid[r][0].material = mats.wall;
        }
        if (cols - 1 < static_cast<int>(grid[r].size()))
        {
            grid[r][cols - 1].type = CellType::Wall;
            grid[r][cols - 1].material = mats.wall;
        }
    }

    if (cols >= 4)
    {
        int gx = cols / 2;
        grid[0][gx].type = CellType::Floor; grid[0][gx].material = mats.floor;
        grid[0][gx + 1].type = CellType::Floor; grid[0][gx + 1].material = mats.floor;
    }
}

// Suburban Sprawl: Floor backdrop with scattered 1x1 and 2x2 Buildings at
// minimum spacing. Sparse half-square fences pointing at nearby houses.
inline void generateSuburbanSprawl(
    std::vector<std::vector<CityCell>>& grid,
    int cols, int rows,
    const GeneratorMaterials& mats,
    uint32_t seed)
{
    std::mt19937 rng(seed);
    std::uniform_real_distribution<double> prob(0.0, 1.0);

    clearGrid(grid, cols, rows);

    for (int r = 0; r < rows && r < static_cast<int>(grid.size()); ++r)
        for (int c = 0; c < cols && c < static_cast<int>(grid[r].size()); ++c)
        {
            grid[r][c].type = CellType::Floor;
            grid[r][c].material = mats.floor;
        }

    int target = std::max(3, (cols * rows) / 18);
    int placed = 0;
    for (int attempt = 0; attempt < target * 12 && placed < target; ++attempt)
    {
        int w = std::uniform_int_distribution<int>(1, 2)(rng);
        int h = std::uniform_int_distribution<int>(1, 2)(rng);
        int x = std::uniform_int_distribution<int>(0, cols - w)(rng);
        int y = std::uniform_int_distribution<int>(0, rows - h)(rng);

        bool clear = true;
        for (int r = y - 1; r <= y + h && clear; ++r)
            for (int c = x - 1; c <= x + w && clear; ++c)
            {
                if (r < 0 || r >= rows || c < 0 || c >= cols) continue;
                CellType t = grid[r][c].type;
                if (t == CellType::Building || t == CellType::WindowlessBuilding)
                    clear = false;
            }
        if (!clear) continue;

        for (int r = y; r < y + h; ++r)
            for (int c = x; c < x + w; ++c)
                grid[r][c].type = CellType::Building;
        ++placed;
    }

    static const int dc[] = {0, 0, 1, -1};
    static const int dr[] = {1, -1, 0, 0};
    static const CellType halfFor[] = {
        CellType::HalfSquareN, CellType::HalfSquareS,
        CellType::HalfSquareE, CellType::HalfSquareW
    };
    for (int r = 0; r < rows && r < static_cast<int>(grid.size()); ++r)
    {
        for (int c = 0; c < cols && c < static_cast<int>(grid[r].size()); ++c)
        {
            if (grid[r][c].type != CellType::Floor) continue;
            if (prob(rng) > 0.10) continue;
            for (int k = 0; k < 4; ++k)
            {
                int nr = r + dr[k], nc = c + dc[k];
                if (nr < 0 || nr >= rows || nc < 0 || nc >= cols) continue;
                if (nr >= static_cast<int>(grid.size())) continue;
                if (nc >= static_cast<int>(grid[nr].size())) continue;
                CellType nt = grid[nr][nc].type;
                if (nt == CellType::Building || nt == CellType::WindowlessBuilding)
                {
                    grid[r][c].type = halfFor[k];
                    grid[r][c].material = mats.wall;
                    break;
                }
            }
        }
    }
}

// Walled Compound: Wall ring (with gate gaps on two sides), Floor courtyard,
// several Buildings inside.
inline void generateWalledCompound(
    std::vector<std::vector<CityCell>>& grid,
    int cols, int rows,
    const GeneratorMaterials& mats,
    uint32_t seed)
{
    std::mt19937 rng(seed);
    std::uniform_real_distribution<double> prob(0.0, 1.0);

    clearGrid(grid, cols, rows);

    for (int r = 0; r < rows && r < static_cast<int>(grid.size()); ++r)
        for (int c = 0; c < cols && c < static_cast<int>(grid[r].size()); ++c)
        {
            grid[r][c].type = CellType::Floor;
            grid[r][c].material = mats.floor;
        }

    for (int c = 0; c < cols; ++c)
    {
        if (0 < static_cast<int>(grid.size()) && c < static_cast<int>(grid[0].size()))
        {
            grid[0][c].type = CellType::Wall; grid[0][c].material = mats.wall;
        }
        if (rows - 1 < static_cast<int>(grid.size()) && c < static_cast<int>(grid[rows - 1].size()))
        {
            grid[rows - 1][c].type = CellType::Wall; grid[rows - 1][c].material = mats.wall;
        }
    }
    for (int r = 0; r < rows && r < static_cast<int>(grid.size()); ++r)
    {
        if (!grid[r].empty())
        {
            grid[r][0].type = CellType::Wall; grid[r][0].material = mats.wall;
        }
        if (cols - 1 < static_cast<int>(grid[r].size()))
        {
            grid[r][cols - 1].type = CellType::Wall; grid[r][cols - 1].material = mats.wall;
        }
    }

    if (cols >= 4)
    {
        int gx = cols / 2;
        grid[0][gx].type = CellType::Floor; grid[0][gx].material = mats.floor;
        grid[rows - 1][gx].type = CellType::Floor; grid[rows - 1][gx].material = mats.floor;
    }
    if (rows >= 4)
    {
        int gy = rows / 2;
        grid[gy][0].type = CellType::Floor; grid[gy][0].material = mats.floor;
        grid[gy][cols - 1].type = CellType::Floor; grid[gy][cols - 1].material = mats.floor;
    }

    int innerX0 = 2, innerY0 = 2;
    int innerX1 = cols - 3, innerY1 = rows - 3;
    if (innerX1 < innerX0 || innerY1 < innerY0) return;

    int target = std::max(1, (cols * rows) / 45);
    int placed = 0;
    for (int attempt = 0; attempt < target * 20 && placed < target; ++attempt)
    {
        int w = std::uniform_int_distribution<int>(2, 4)(rng);
        int h = std::uniform_int_distribution<int>(2, 4)(rng);
        if (w > innerX1 - innerX0 + 1 || h > innerY1 - innerY0 + 1) continue;

        int x = std::uniform_int_distribution<int>(innerX0, innerX1 - w + 1)(rng);
        int y = std::uniform_int_distribution<int>(innerY0, innerY1 - h + 1)(rng);

        bool clear = true;
        for (int r = y - 1; r <= y + h && clear; ++r)
            for (int c = x - 1; c <= x + w && clear; ++c)
            {
                if (r < 0 || r >= rows || c < 0 || c >= cols) continue;
                CellType t = grid[r][c].type;
                if (t == CellType::Building || t == CellType::WindowlessBuilding)
                    clear = false;
            }
        if (!clear) continue;

        CellType bt = (prob(rng) < 0.25)
            ? CellType::WindowlessBuilding : CellType::Building;
        for (int r = y; r < y + h; ++r)
            for (int c = x; c < x + w; ++c)
                grid[r][c].type = bt;
        ++placed;
    }
}

// Voronoi Districts: partition the grid into ~N regions around random seed
// points. Each region's interior becomes one Building (group); region borders
// become Floor streets that naturally follow the grid.
inline void generateVoronoiDistricts(
    std::vector<std::vector<CityCell>>& grid,
    int cols, int rows,
    const GeneratorMaterials& mats,
    uint32_t seed)
{
    std::mt19937 rng(seed);
    std::uniform_real_distribution<double> prob(0.0, 1.0);

    clearGrid(grid, cols, rows);

    int n = std::max(3, (cols * rows) / 70);
    std::vector<std::pair<int, int>> seeds(n);
    for (int i = 0; i < n; ++i)
        seeds[i] = { std::uniform_int_distribution<int>(0, cols - 1)(rng),
                     std::uniform_int_distribution<int>(0, rows - 1)(rng) };

    std::vector<std::vector<int>> regionId(rows, std::vector<int>(cols, 0));
    for (int r = 0; r < rows; ++r)
    {
        for (int c = 0; c < cols; ++c)
        {
            int best = 0;
            long long bestDist = -1;
            for (int i = 0; i < n; ++i)
            {
                long long dx = c - seeds[i].first;
                long long dy = r - seeds[i].second;
                long long d = dx * dx + dy * dy;
                if (bestDist < 0 || d < bestDist) { bestDist = d; best = i; }
            }
            regionId[r][c] = best;
        }
    }

    std::vector<int> regionKind(n);
    for (int i = 0; i < n; ++i)
        regionKind[i] = (prob(rng) < 0.25) ? 1 : 0;

    static const int dr[] = {0, 0, 1, -1};
    static const int dc[] = {1, -1, 0, 0};

    for (int r = 0; r < rows && r < static_cast<int>(grid.size()); ++r)
    {
        for (int c = 0; c < cols && c < static_cast<int>(grid[r].size()); ++c)
        {
            int rid = regionId[r][c];
            bool border = false;
            for (int k = 0; k < 4 && !border; ++k)
            {
                int nr = r + dr[k], nc = c + dc[k];
                if (nr < 0 || nr >= rows || nc < 0 || nc >= cols) continue;
                if (regionId[nr][nc] != rid) border = true;
            }
            if (border)
            {
                grid[r][c].type = CellType::Floor;
                grid[r][c].material = mats.floor;
            }
            else
            {
                grid[r][c].type = (regionKind[rid] == 1)
                    ? CellType::WindowlessBuilding : CellType::Building;
            }
        }
    }
}

} // namespace cityplanner
