#pragma once

#include "TileMapGeometry.h"

#include <algorithm>
#include <cstdint>
#include <random>
#include <stack>
#include <string>
#include <vector>

namespace tilemap
{

inline void generateCellularAutomata(
    std::vector<std::vector<Tile>>& grid,
    int cols, int rows,
    const TileMaterials& materials,
    uint32_t seed)
{
    std::mt19937 rng(seed);
    std::uniform_int_distribution<int> dist(0, 99);

    for (int y = 0; y < rows; ++y)
        for (int x = 0; x < cols; ++x)
            grid[y][x].type = (dist(rng) < 45) ? TileType::Room : TileType::Empty;

    std::vector<std::vector<TileType>> buf(rows, std::vector<TileType>(cols));

    for (int iter = 0; iter < 5; ++iter)
    {
        for (int y = 0; y < rows; ++y)
        {
            for (int x = 0; x < cols; ++x)
            {
                int neighbors = 0;
                for (int dy = -1; dy <= 1; ++dy)
                    for (int dx = -1; dx <= 1; ++dx)
                    {
                        if (dx == 0 && dy == 0) continue;
                        int nx = x + dx, ny = y + dy;
                        if (nx >= 0 && nx < cols && ny >= 0 && ny < rows
                            && isSolidType(grid[ny][nx].type))
                            ++neighbors;
                    }

                if (neighbors >= 5)
                    buf[y][x] = TileType::Room;
                else if (neighbors < 4)
                    buf[y][x] = TileType::Empty;
                else
                    buf[y][x] = grid[y][x].type;
            }
        }

        for (int y = 0; y < rows; ++y)
            for (int x = 0; x < cols; ++x)
                grid[y][x].type = buf[y][x];
    }

    for (int y = 0; y < rows; ++y)
        for (int x = 0; x < cols; ++x)
            if (isSolidType(grid[y][x].type))
                grid[y][x].materials = materials;
}

inline void generateBSPDungeon(
    std::vector<std::vector<Tile>>& grid,
    int cols, int rows,
    const TileMaterials& materials,
    uint32_t seed)
{
    std::mt19937 rng(seed);

    for (int y = 0; y < rows; ++y)
        for (int x = 0; x < cols; ++x)
            grid[y][x].type = TileType::Empty;

    struct Node { int x0, y0, x1, y1; int left, right; };
    std::vector<Node> nodes;
    nodes.reserve(64);
    nodes.push_back({0, 0, cols - 1, rows - 1, -1, -1});

    const int minLeaf = 4;

    std::stack<int> toSplit;
    toSplit.push(0);

    while (!toSplit.empty())
    {
        int idx = toSplit.top();
        toSplit.pop();

        Node cur = nodes[idx];
        int w = cur.x1 - cur.x0 + 1;
        int h = cur.y1 - cur.y0 + 1;

        if (w < minLeaf * 2 && h < minLeaf * 2)
            continue;

        bool splitH;
        if (w < minLeaf * 2)
            splitH = true;
        else if (h < minLeaf * 2)
            splitH = false;
        else
            splitH = std::uniform_int_distribution<int>(0, 1)(rng) == 0;

        if (splitH)
        {
            int lo = cur.y0 + minLeaf;
            int hi = cur.y1 - minLeaf + 1;
            if (lo > hi) continue;
            int split = std::uniform_int_distribution<int>(lo, hi)(rng);
            int li = static_cast<int>(nodes.size());
            nodes.push_back({cur.x0, cur.y0, cur.x1, split - 1, -1, -1});
            int ri = static_cast<int>(nodes.size());
            nodes.push_back({cur.x0, split, cur.x1, cur.y1, -1, -1});
            nodes[idx].left = li;
            nodes[idx].right = ri;
            toSplit.push(li);
            toSplit.push(ri);
        }
        else
        {
            int lo = cur.x0 + minLeaf;
            int hi = cur.x1 - minLeaf + 1;
            if (lo > hi) continue;
            int split = std::uniform_int_distribution<int>(lo, hi)(rng);
            int li = static_cast<int>(nodes.size());
            nodes.push_back({cur.x0, cur.y0, split - 1, cur.y1, -1, -1});
            int ri = static_cast<int>(nodes.size());
            nodes.push_back({split, cur.y0, cur.x1, cur.y1, -1, -1});
            nodes[idx].left = li;
            nodes[idx].right = ri;
            toSplit.push(li);
            toSplit.push(ri);
        }
    }

    struct RoomRect { int x0, y0, x1, y1; };
    std::vector<RoomRect> rooms(nodes.size(), {-1, -1, -1, -1});

    auto fillRect = [&](int x0, int y0, int x1, int y1) {
        for (int y = std::max(0, y0); y <= std::min(rows - 1, y1); ++y)
            for (int x = std::max(0, x0); x <= std::min(cols - 1, x1); ++x)
            {
                grid[y][x].type = TileType::Room;
                grid[y][x].materials = materials;
            }
    };

    for (int i = 0; i < static_cast<int>(nodes.size()); ++i)
    {
        if (nodes[i].left != -1)
            continue;

        Node n = nodes[i];
        int w = n.x1 - n.x0 + 1;
        int h = n.y1 - n.y0 + 1;
        int rwMin = std::max(2, w / 2);
        int rwMax = std::max(rwMin, w - 1);
        int rhMin = std::max(2, h / 2);
        int rhMax = std::max(rhMin, h - 1);
        int rw = std::uniform_int_distribution<int>(rwMin, rwMax)(rng);
        int rh = std::uniform_int_distribution<int>(rhMin, rhMax)(rng);
        int rxMax = std::max(n.x0, n.x1 - rw + 1);
        int ryMax = std::max(n.y0, n.y1 - rh + 1);
        int rx = std::uniform_int_distribution<int>(n.x0, rxMax)(rng);
        int ry = std::uniform_int_distribution<int>(n.y0, ryMax)(rng);

        rooms[i] = {rx, ry, rx + rw - 1, ry + rh - 1};
        fillRect(rx, ry, rx + rw - 1, ry + rh - 1);
    }

    std::function<RoomRect(int)> getRoom = [&](int idx) -> RoomRect {
        if (rooms[idx].x0 != -1)
            return rooms[idx];
        if (nodes[idx].left != -1)
        {
            auto l = getRoom(nodes[idx].left);
            auto r = getRoom(nodes[idx].right);
            return std::uniform_int_distribution<int>(0, 1)(rng) == 0 ? l : r;
        }
        return {-1, -1, -1, -1};
    };

    for (int i = 0; i < static_cast<int>(nodes.size()); ++i)
    {
        if (nodes[i].left == -1)
            continue;

        auto l = getRoom(nodes[i].left);
        auto r = getRoom(nodes[i].right);
        if (l.x0 == -1 || r.x0 == -1)
            continue;

        int lx = (l.x0 + l.x1) / 2;
        int ly = (l.y0 + l.y1) / 2;
        int rx = (r.x0 + r.x1) / 2;
        int ry = (r.y0 + r.y1) / 2;

        if (std::uniform_int_distribution<int>(0, 1)(rng) == 0)
        {
            fillRect(std::min(lx, rx), ly, std::max(lx, rx), ly);
            fillRect(rx, std::min(ly, ry), rx, std::max(ly, ry));
        }
        else
        {
            fillRect(lx, std::min(ly, ry), lx, std::max(ly, ry));
            fillRect(std::min(lx, rx), ry, std::max(lx, rx), ry);
        }
    }
}

inline void generateLSystem(
    std::vector<std::vector<Tile>>& grid,
    int cols, int rows,
    const TileMaterials& materials,
    uint32_t seed)
{
    std::mt19937 rng(seed);

    for (int y = 0; y < rows; ++y)
        for (int x = 0; x < cols; ++x)
            grid[y][x].type = TileType::Empty;

    struct Rule { const char* from; const char* to; };
    Rule ruleSets[][2] = {
        {{"F", "F+F-F-F+F"}, {"", ""}},
        {{"F", "F-F+F+F-F"}, {"", ""}},
        {{"F", "F+F-F+F-F"}, {"", ""}},
        {{"F", "FF+F+F+F+FF"}, {"", ""}},
        {{"F", "F+FF--F+F"}, {"", ""}},
    };
    int ruleSet = std::uniform_int_distribution<int>(0, 4)(rng);
    const char* rule = ruleSets[ruleSet][0].to;

    int iterations = 3;
    int gridMax = std::max(cols, rows);
    if (gridMax >= 32) iterations = 2;
    if (gridMax < 12) iterations = 4;

    std::string current = "F";
    for (int i = 0; i < iterations; ++i)
    {
        std::string next;
        for (char c : current)
        {
            if (c == 'F')
                next += rule;
            else
                next += c;
        }
        current = next;
        if (current.size() > 100000)
            break;
    }

    int startDir = std::uniform_int_distribution<int>(0, 3)(rng);
    static const int dirTable[][2] = {{1,0},{0,1},{-1,0},{0,-1}};
    int dx = dirTable[startDir][0];
    int dy = dirTable[startDir][1];

    int cx = std::uniform_int_distribution<int>(cols / 4, cols * 3 / 4)(rng);
    int cy = std::uniform_int_distribution<int>(rows / 4, rows * 3 / 4)(rng);

    for (char c : current)
    {
        if (c == 'F')
        {
            if (cx >= 0 && cx < cols && cy >= 0 && cy < rows)
            {
                grid[cy][cx].type = TileType::Room;
                grid[cy][cx].materials = materials;
            }
            cx += dx;
            cy += dy;
        }
        else if (c == '+')
        {
            int tmp = dx;
            dx = -dy;
            dy = tmp;
        }
        else if (c == '-')
        {
            int tmp = dx;
            dx = dy;
            dy = -tmp;
        }
    }
}

inline void generateMaze(
    std::vector<std::vector<Tile>>& grid,
    int cols, int rows,
    const TileMaterials& materials,
    uint32_t seed)
{
    std::mt19937 rng(seed);

    for (int y = 0; y < rows; ++y)
        for (int x = 0; x < cols; ++x)
        {
            grid[y][x].type = TileType::Room;
            grid[y][x].materials = materials;
        }

    std::vector<std::vector<bool>> visited(rows, std::vector<bool>(cols, false));

    struct Cell { int x, y; };
    std::stack<Cell> stack;

    int sx = 1, sy = 1;
    if (sx < cols && sy < rows)
    {
        grid[sy][sx].type = TileType::Empty;
        visited[sy][sx] = true;
        stack.push({sx, sy});
    }

    const int dirs[][2] = {{0, -2}, {0, 2}, {-2, 0}, {2, 0}};

    while (!stack.empty())
    {
        auto cur = stack.top();

        int unvisited[4];
        int count = 0;
        for (int d = 0; d < 4; ++d)
        {
            int nx = cur.x + dirs[d][0];
            int ny = cur.y + dirs[d][1];
            if (nx >= 0 && nx < cols && ny >= 0 && ny < rows && !visited[ny][nx])
                unvisited[count++] = d;
        }

        if (count == 0)
        {
            stack.pop();
            continue;
        }

        int pick = std::uniform_int_distribution<int>(0, count - 1)(rng);
        int d = unvisited[pick];
        int nx = cur.x + dirs[d][0];
        int ny = cur.y + dirs[d][1];
        int wx = cur.x + dirs[d][0] / 2;
        int wy = cur.y + dirs[d][1] / 2;

        grid[wy][wx].type = TileType::Empty;
        grid[ny][nx].type = TileType::Empty;
        visited[ny][nx] = true;
        stack.push({nx, ny});
    }
}

inline void generateDeathmatch(
    std::vector<std::vector<Tile>>& grid,
    int cols, int rows,
    const TileMaterials& materials,
    uint32_t seed)
{
    std::mt19937 rng(seed);

    for (int y = 0; y < rows; ++y)
        for (int x = 0; x < cols; ++x)
            grid[y][x].type = TileType::Empty;

    auto fillRect = [&](int x0, int y0, int x1, int y1) {
        for (int y = std::max(0, y0); y <= std::min(rows - 1, y1); ++y)
            for (int x = std::max(0, x0); x <= std::min(cols - 1, x1); ++x)
            {
                grid[y][x].type = TileType::Room;
                grid[y][x].materials = materials;
            }
    };

    auto corridor = [&](int x0, int y0, int x1, int y1) {
        int minX = std::min(x0, x1), maxX = std::max(x0, x1);
        int minY = std::min(y0, y1), maxY = std::max(y0, y1);
        if (std::uniform_int_distribution<int>(0, 1)(rng) == 0)
        {
            fillRect(minX, y0, maxX, y0);
            fillRect(x1, minY, x1, maxY);
        }
        else
        {
            fillRect(x0, minY, x0, maxY);
            fillRect(minX, y1, maxX, y1);
        }
    };

    int numRooms = std::uniform_int_distribution<int>(5, 8)(rng);
    int minSize = std::max(2, std::min(cols, rows) / 6);
    int maxSize = std::max(3, std::min(cols, rows) / 4);

    struct Room { int cx, cy, x0, y0, x1, y1; };
    std::vector<Room> roomList;

    for (int i = 0; i < numRooms; ++i)
    {
        int rw = std::uniform_int_distribution<int>(minSize, maxSize)(rng);
        int rh = std::uniform_int_distribution<int>(minSize, maxSize)(rng);
        int rx = std::uniform_int_distribution<int>(1, std::max(1, cols - rw - 1))(rng);
        int ry = std::uniform_int_distribution<int>(1, std::max(1, rows - rh - 1))(rng);
        fillRect(rx, ry, rx + rw - 1, ry + rh - 1);
        roomList.push_back({rx + rw / 2, ry + rh / 2, rx, ry, rx + rw - 1, ry + rh - 1});
    }

    if (roomList.size() < 2)
        return;

    for (int i = 0; i < static_cast<int>(roomList.size()); ++i)
    {
        int next = (i + 1) % static_cast<int>(roomList.size());
        corridor(roomList[i].cx, roomList[i].cy, roomList[next].cx, roomList[next].cy);
    }

    int extras = std::uniform_int_distribution<int>(1, std::max(1, numRooms / 2))(rng);
    for (int i = 0; i < extras; ++i)
    {
        int a = std::uniform_int_distribution<int>(0, static_cast<int>(roomList.size()) - 1)(rng);
        int b = std::uniform_int_distribution<int>(0, static_cast<int>(roomList.size()) - 1)(rng);
        if (a != b)
            corridor(roomList[a].cx, roomList[a].cy, roomList[b].cx, roomList[b].cy);
    }
}

inline void generateRandomNoise(
    std::vector<std::vector<Tile>>& grid,
    int cols, int rows,
    const TileMaterials& materials,
    uint32_t seed)
{
    std::mt19937 rng(seed);
    std::uniform_int_distribution<int> dist(0, 99);

    for (int y = 0; y < rows; ++y)
        for (int x = 0; x < cols; ++x)
        {
            if (dist(rng) < 50)
            {
                grid[y][x].type = TileType::Room;
                grid[y][x].materials = materials;
            }
            else
            {
                grid[y][x].type = TileType::Empty;
            }
        }
}

} // namespace tilemap
