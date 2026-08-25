#include "OpeningSolver.h"

#include <algorithm>
#include <cmath>
#include <limits>

#include "imodelsurface.h"

namespace brush
{

namespace algorithm
{

namespace
{

const double DEGENERATE_AREA = 1e-9;
const double BARYCENTRIC_TOLERANCE = 0.02;
const std::size_t GRID_PADDING = 2;
const double CONTAINMENT_TOLERANCE = 0.01;

struct LocalTriangle
{
    Vector3 a;
    Vector3 b;
    Vector3 c;
};

struct DepthGrid
{
    std::size_t width = 0;
    std::size_t height = 0;
    double originU = 0;
    double originV = 0;
    double cell = 1;
    std::vector<double> minDepth;
    std::vector<double> maxDepth;

    std::size_t index(std::size_t i, std::size_t j) const
    {
        return j * width + i;
    }

    bool covered(std::size_t k) const
    {
        return minDepth[k] <= maxDepth[k];
    }
};

Vector3 toLocal(const OpeningFrame& frame, const Vector3& world)
{
    Vector3 delta = world - frame.origin;

    return Vector3(delta.dot(frame.run), delta.dot(frame.up), delta.dot(frame.normal));
}

std::vector<LocalTriangle> collectTriangles(const model::IModel& model,
    const Matrix4& modelToWorld, const OpeningFrame& frame)
{
    std::vector<LocalTriangle> triangles;

    for (int surfaceNum = 0; surfaceNum < model.getSurfaceCount(); ++surfaceNum)
    {
        const model::IModelSurface& surface = model.getSurface(surfaceNum);

        triangles.reserve(triangles.size() + surface.getNumTriangles());

        for (int polyNum = 0; polyNum < surface.getNumTriangles(); ++polyNum)
        {
            model::ModelPolygon poly = surface.getPolygon(polyNum);

            LocalTriangle triangle;
            triangle.a = toLocal(frame, modelToWorld.transformPoint(poly.a.vertex));
            triangle.b = toLocal(frame, modelToWorld.transformPoint(poly.b.vertex));
            triangle.c = toLocal(frame, modelToWorld.transformPoint(poly.c.vertex));

            triangles.push_back(triangle);
        }
    }

    return triangles;
}

void accumulate(DepthGrid& grid, std::size_t k, double low, double high)
{
    if (low < grid.minDepth[k])
    {
        grid.minDepth[k] = low;
    }

    if (high > grid.maxDepth[k])
    {
        grid.maxDepth[k] = high;
    }
}

bool triangleOverlapsCell(const double* triU, const double* triV,
    double minU, double minV, double maxU, double maxV)
{
    for (int edge = 0; edge < 3; ++edge)
    {
        int next = (edge + 1) % 3;
        double axisU = -(triV[next] - triV[edge]);
        double axisV = triU[next] - triU[edge];

        if (std::abs(axisU) < 1e-12 && std::abs(axisV) < 1e-12)
        {
            continue;
        }

        double triLow = std::numeric_limits<double>::max();
        double triHigh = std::numeric_limits<double>::lowest();

        for (int corner = 0; corner < 3; ++corner)
        {
            double value = axisU * triU[corner] + axisV * triV[corner];
            triLow = std::min(triLow, value);
            triHigh = std::max(triHigh, value);
        }

        double cellLow = std::numeric_limits<double>::max();
        double cellHigh = std::numeric_limits<double>::lowest();

        for (int corner = 0; corner < 4; ++corner)
        {
            double value = axisU * (corner & 1 ? maxU : minU) + axisV * (corner & 2 ? maxV : minV);
            cellLow = std::min(cellLow, value);
            cellHigh = std::max(cellHigh, value);
        }

        if (cellHigh < triLow || cellLow > triHigh)
        {
            return false;
        }
    }

    return true;
}

void rasterise(const LocalTriangle& triangle, DepthGrid& grid)
{
    double minU = std::min({ triangle.a.x(), triangle.b.x(), triangle.c.x() });
    double maxU = std::max({ triangle.a.x(), triangle.b.x(), triangle.c.x() });
    double minV = std::min({ triangle.a.y(), triangle.b.y(), triangle.c.y() });
    double maxV = std::max({ triangle.a.y(), triangle.b.y(), triangle.c.y() });
    double minN = std::min({ triangle.a.z(), triangle.b.z(), triangle.c.z() });
    double maxN = std::max({ triangle.a.z(), triangle.b.z(), triangle.c.z() });

    int firstI = static_cast<int>(std::floor((minU - grid.originU) / grid.cell));
    int lastI = static_cast<int>(std::floor((maxU - grid.originU) / grid.cell));
    int firstJ = static_cast<int>(std::floor((minV - grid.originV) / grid.cell));
    int lastJ = static_cast<int>(std::floor((maxV - grid.originV) / grid.cell));

    firstI = std::max(firstI, 0);
    firstJ = std::max(firstJ, 0);
    lastI = std::min<int>(lastI, static_cast<int>(grid.width) - 1);
    lastJ = std::min<int>(lastJ, static_cast<int>(grid.height) - 1);

    if (firstI > lastI || firstJ > lastJ)
    {
        return;
    }

    double au = triangle.a.x();
    double av = triangle.a.y();
    double bu = triangle.b.x();
    double bv = triangle.b.y();
    double cu = triangle.c.x();
    double cv = triangle.c.y();

    double denominator = (bv - av) * (cu - au) - (bu - au) * (cv - av);
    bool degenerate = std::abs(denominator) < DEGENERATE_AREA;

    const double triU[3] = { au, bu, cu };
    const double triV[3] = { av, bv, cv };

    for (int i = firstI; i <= lastI; ++i)
    {
        double cellMinU = grid.originU + i * grid.cell;
        double pointU = cellMinU + grid.cell * 0.5;

        for (int j = firstJ; j <= lastJ; ++j)
        {
            double cellMinV = grid.originV + j * grid.cell;

            if (!triangleOverlapsCell(triU, triV, cellMinU, cellMinV,
                cellMinU + grid.cell, cellMinV + grid.cell))
            {
                continue;
            }

            double pointV = cellMinV + grid.cell * 0.5;
            std::size_t k = grid.index(i, j);

            if (degenerate)
            {
                accumulate(grid, k, minN, maxN);
                continue;
            }

            double weightB = ((pointV - av) * (cu - au) - (pointU - au) * (cv - av)) / denominator;
            double weightC = ((pointU - au) * (bv - av) - (pointV - av) * (bu - au)) / denominator;

            if (weightB < -BARYCENTRIC_TOLERANCE || weightC < -BARYCENTRIC_TOLERANCE ||
                weightB + weightC > 1.0 + BARYCENTRIC_TOLERANCE)
            {
                accumulate(grid, k, minN, maxN);
                continue;
            }

            double depth = triangle.a.z() + weightB * (triangle.b.z() - triangle.a.z()) +
                weightC * (triangle.c.z() - triangle.a.z());

            accumulate(grid, k, depth, depth);
        }
    }
}

void fillBracketed(std::vector<char>& grid, std::size_t width, std::size_t height)
{
    std::vector<char> before(width * height, 0);
    std::vector<char> after(width * height, 0);
    std::vector<char> below(width * height, 0);
    std::vector<char> above(width * height, 0);

    for (std::size_t j = 0; j < height; ++j)
    {
        bool seen = false;

        for (std::size_t i = 0; i < width; ++i)
        {
            std::size_t k = j * width + i;
            before[k] = seen ? 1 : 0;
            seen = seen || grid[k];
        }

        seen = false;

        for (std::size_t i = width; i-- > 0;)
        {
            std::size_t k = j * width + i;
            after[k] = seen ? 1 : 0;
            seen = seen || grid[k];
        }
    }

    for (std::size_t i = 0; i < width; ++i)
    {
        bool seen = false;

        for (std::size_t j = 0; j < height; ++j)
        {
            std::size_t k = j * width + i;
            below[k] = seen ? 1 : 0;
            seen = seen || grid[k];
        }

        seen = false;

        for (std::size_t j = height; j-- > 0;)
        {
            std::size_t k = j * width + i;
            above[k] = seen ? 1 : 0;
            seen = seen || grid[k];
        }
    }

    for (std::size_t k = 0; k < grid.size(); ++k)
    {
        if (grid[k])
        {
            continue;
        }

        if ((before[k] && after[k]) || (below[k] && above[k]))
        {
            grid[k] = 1;
        }
    }
}

std::vector<char> selectOpenings(const std::vector<char>& occupied,
    const std::vector<double>& coverage, double apertureLimit, std::size_t width,
    std::size_t height, double cellArea, double minOpeningArea,
    std::vector<char>& dropped, std::size_t& droppedCount)
{
    std::vector<char> visited(occupied.size(), 0);
    std::vector<char> result(occupied.size(), 0);

    dropped.assign(occupied.size(), 0);
    droppedCount = 0;
    std::vector<std::size_t> stack;
    std::vector<std::size_t> cells;

    for (std::size_t seed = 0; seed < occupied.size(); ++seed)
    {
        if (!occupied[seed] || visited[seed])
        {
            continue;
        }

        cells.clear();
        stack.clear();
        stack.push_back(seed);
        visited[seed] = 1;

        while (!stack.empty())
        {
            std::size_t k = stack.back();
            stack.pop_back();
            cells.push_back(k);

            std::size_t i = k % width;
            std::size_t j = k / width;

            auto visit = [&](std::size_t neighbour)
            {
                if (occupied[neighbour] && !visited[neighbour])
                {
                    visited[neighbour] = 1;
                    stack.push_back(neighbour);
                }
            };

            if (i > 0) visit(k - 1);
            if (i + 1 < width) visit(k + 1);
            if (j > 0) visit(k - width);
            if (j + 1 < height) visit(k + width);
        }

        std::vector<char> component(occupied.size(), 0);

        for (std::size_t k : cells)
        {
            component[k] = 1;
        }

        fillBracketed(component, width, height);

        double aperture = 0;

        for (std::size_t k = 0; k < component.size(); ++k)
        {
            if (component[k] && coverage[k] < apertureLimit)
            {
                aperture += cellArea;
            }
        }

        if (aperture < minOpeningArea)
        {
            for (std::size_t k : cells)
            {
                dropped[k] = 1;
            }

            ++droppedCount;
            continue;
        }

        for (std::size_t k = 0; k < component.size(); ++k)
        {
            if (component[k])
            {
                result[k] = 1;
            }
        }
    }

    return result;
}

std::vector<polygon::Ring> gridToRings(const std::vector<char>& grid, const DepthGrid& shape)
{
    std::vector<polygon::Ring> quads;

    for (std::size_t j = 0; j < shape.height; ++j)
    {
        std::size_t i = 0;

        while (i < shape.width)
        {
            if (!grid[shape.index(i, j)])
            {
                ++i;
                continue;
            }

            std::size_t start = i;

            while (i < shape.width && grid[shape.index(i, j)])
            {
                ++i;
            }

            double lowU = shape.originU + start * shape.cell;
            double highU = shape.originU + i * shape.cell;
            double lowV = shape.originV + j * shape.cell;
            double highV = shape.originV + (j + 1) * shape.cell;

            polygon::Ring quad;
            quad.push_back(Vector2(lowU, lowV));
            quad.push_back(Vector2(highU, lowV));
            quad.push_back(Vector2(highU, highV));
            quad.push_back(Vector2(lowU, highV));

            quads.push_back(quad);
        }
    }

    return polygon::repair(quads, 0.0);
}

double ringsArea(const std::vector<polygon::Ring>& rings)
{
    double total = 0;

    for (const polygon::Ring& ring : rings)
    {
        total += std::abs(polygon::ringArea(ring));
    }

    return total;
}

std::vector<Vector2> collectSilhouetteVertices(const std::vector<LocalTriangle>& triangles,
    const std::vector<char>& hole, const DepthGrid& grid, double back, double front)
{
    std::vector<char> boundary(hole.size(), 0);

    for (std::size_t j = 0; j < grid.height; ++j)
    {
        for (std::size_t i = 0; i < grid.width; ++i)
        {
            std::size_t k = grid.index(i, j);

            if (!hole[k])
            {
                continue;
            }

            bool edge = i == 0 || j == 0 || i + 1 == grid.width || j + 1 == grid.height ||
                !hole[k - 1] || !hole[k + 1] || !hole[k - grid.width] || !hole[k + grid.width];

            if (edge)
            {
                boundary[k] = 1;
            }
        }
    }

    std::vector<Vector2> points;

    for (const LocalTriangle& triangle : triangles)
    {
        for (const Vector3* point : { &triangle.a, &triangle.b, &triangle.c })
        {
            if (point->z() < back || point->z() > front)
            {
                continue;
            }

            int i = static_cast<int>(std::floor((point->x() - grid.originU) / grid.cell));
            int j = static_cast<int>(std::floor((point->y() - grid.originV) / grid.cell));

            if (i < 1 || j < 1 || i + 1 >= static_cast<int>(grid.width) ||
                j + 1 >= static_cast<int>(grid.height))
            {
                continue;
            }

            bool near = false;

            for (int dj = -1; dj <= 1 && !near; ++dj)
            {
                for (int di = -1; di <= 1 && !near; ++di)
                {
                    near = boundary[grid.index(i + di, j + dj)] != 0;
                }
            }

            if (near)
            {
                points.push_back(Vector2(point->x(), point->y()));
            }
        }
    }

    std::sort(points.begin(), points.end(), [](const Vector2& a, const Vector2& b)
    {
        return a.x() < b.x();
    });

    return points;
}

std::vector<polygon::Ring> snapRings(const std::vector<polygon::Ring>& rings,
    const std::vector<Vector2>& points, double tolerance)
{
    if (tolerance <= 0 || points.empty())
    {
        return rings;
    }

    std::vector<polygon::Ring> snapped;
    snapped.reserve(rings.size());

    for (const polygon::Ring& ring : rings)
    {
        polygon::Ring result;
        result.reserve(ring.size());

        for (const Vector2& vertex : ring)
        {
            auto lower = std::lower_bound(points.begin(), points.end(), vertex.x() - tolerance,
                [](const Vector2& point, double value) { return point.x() < value; });

            Vector2 best = vertex;
            double bestDistance = tolerance;

            for (auto it = lower; it != points.end() && it->x() <= vertex.x() + tolerance; ++it)
            {
                double distance = (*it - vertex).getLength();

                if (distance < bestDistance)
                {
                    bestDistance = distance;
                    best = *it;
                }
            }

            result.push_back(best);
        }

        snapped.push_back(result);
    }

    return snapped;
}

polygon::Ring convexHull(const polygon::Ring& ring)
{
    if (ring.size() < 4)
    {
        return ring;
    }

    polygon::Ring points = ring;

    std::sort(points.begin(), points.end(), [](const Vector2& a, const Vector2& b)
    {
        return a.x() < b.x() || (a.x() == b.x() && a.y() < b.y());
    });

    auto cross = [](const Vector2& o, const Vector2& a, const Vector2& b)
    {
        return (a.x() - o.x()) * (b.y() - o.y()) - (a.y() - o.y()) * (b.x() - o.x());
    };

    polygon::Ring hull(points.size() * 2);
    std::size_t count = 0;

    for (std::size_t i = 0; i < points.size(); ++i)
    {
        while (count >= 2 && cross(hull[count - 2], hull[count - 1], points[i]) <= 0)
        {
            --count;
        }

        hull[count++] = points[i];
    }

    std::size_t lower = count + 1;

    for (std::size_t i = points.size() - 1; i > 0; --i)
    {
        while (count >= lower && cross(hull[count - 2], hull[count - 1], points[i - 1]) <= 0)
        {
            --count;
        }

        hull[count++] = points[i - 1];
    }

    hull.resize(count > 0 ? count - 1 : 0);

    return hull;
}

std::vector<polygon::Ring> hullRings(const std::vector<polygon::Ring>& rings)
{
    std::vector<polygon::Ring> result;
    result.reserve(rings.size());

    for (const polygon::Ring& ring : rings)
    {
        polygon::Ring hull = convexHull(ring);

        if (hull.size() >= 3)
        {
            result.push_back(hull);
        }
    }

    return result;
}

bool ringsContain(const std::vector<polygon::Ring>& rings, const Vector2& point, double tolerance)
{
    bool odd = false;

    for (const polygon::Ring& ring : rings)
    {
        for (std::size_t i = 0, j = ring.size() - 1; i < ring.size(); j = i++)
        {
            const Vector2& current = ring[i];
            const Vector2& previous = ring[j];

            if ((current.y() > point.y()) != (previous.y() > point.y()) &&
                point.x() < (previous.x() - current.x()) * (point.y() - current.y()) /
                    (previous.y() - current.y()) + current.x())
            {
                odd = !odd;
            }
        }
    }

    if (odd)
    {
        return true;
    }

    for (const polygon::Ring& ring : rings)
    {
        for (std::size_t i = 0, j = ring.size() - 1; i < ring.size(); j = i++)
        {
            Vector2 edge = ring[i] - ring[j];
            double lengthSquared = edge.dot(edge);
            double along = lengthSquared < 1e-12 ? 0.0 :
                std::max(0.0, std::min(1.0, (point - ring[j]).dot(edge) / lengthSquared));

            if ((point - (ring[j] + edge * along)).getLengthSquared() <= tolerance * tolerance)
            {
                return true;
            }
        }
    }

    return false;
}

bool ringsCoverSlab(const std::vector<polygon::Ring>& rings,
    const std::vector<LocalTriangle>& triangles, double back, double front, double tolerance)
{
    for (const LocalTriangle& triangle : triangles)
    {
        for (const Vector3* point : { &triangle.a, &triangle.b, &triangle.c })
        {
            if (point->z() < back || point->z() > front)
            {
                continue;
            }

            if (!ringsContain(rings, Vector2(point->x(), point->y()), tolerance))
            {
                return false;
            }
        }
    }

    return true;
}

} // namespace

OpeningFrame buildOpeningFrame(const Vector3& normal, const Vector3& pointOnMidPlane,
    double back, double front)
{
    OpeningFrame frame;

    frame.normal = normal.getNormalised();

    Vector3 reference = std::abs(frame.normal.z()) > 0.9 ? Vector3(0, 1, 0) : Vector3(0, 0, 1);

    frame.up = (reference - frame.normal * reference.dot(frame.normal)).getNormalised();
    frame.run = frame.up.cross(frame.normal);
    frame.origin = pointOnMidPlane;
    frame.back = back;
    frame.front = front;

    return frame;
}

OpeningSolution solveOpening(const model::IModel& model, const Matrix4& modelToWorld,
    const OpeningFrame& frame, const OpeningSettings& settings)
{
    OpeningSolution solution;

    std::vector<LocalTriangle> triangles = collectTriangles(model, modelToWorld, frame);

    if (triangles.empty())
    {
        return solution;
    }

    double minU = std::numeric_limits<double>::max();
    double maxU = std::numeric_limits<double>::lowest();
    double minV = std::numeric_limits<double>::max();
    double maxV = std::numeric_limits<double>::lowest();
    double minN = std::numeric_limits<double>::max();
    double maxN = std::numeric_limits<double>::lowest();

    for (const LocalTriangle& triangle : triangles)
    {
        for (const Vector3* point : { &triangle.a, &triangle.b, &triangle.c })
        {
            minU = std::min(minU, point->x());
            maxU = std::max(maxU, point->x());
            minV = std::min(minV, point->y());
            maxV = std::max(maxV, point->y());
            minN = std::min(minN, point->z());
            maxN = std::max(maxN, point->z());
        }
    }

    solution.modelDepth = maxN - minN;

    DepthGrid grid;
    grid.cell = settings.cell;

    double spanU = maxU - minU;
    double spanV = maxV - minV;
    double largest = std::max(spanU, spanV);

    if (largest <= 0)
    {
        return solution;
    }

    double minimumCell = largest / static_cast<double>(settings.maxCellsPerAxis);

    if (grid.cell < minimumCell)
    {
        grid.cell = minimumCell;
    }

    grid.width = static_cast<std::size_t>(spanU / grid.cell) + 1 + 2 * GRID_PADDING;
    grid.height = static_cast<std::size_t>(spanV / grid.cell) + 1 + 2 * GRID_PADDING;
    grid.originU = minU - GRID_PADDING * grid.cell;
    grid.originV = minV - GRID_PADDING * grid.cell;
    grid.minDepth.assign(grid.width * grid.height, std::numeric_limits<double>::max());
    grid.maxDepth.assign(grid.width * grid.height, std::numeric_limits<double>::lowest());

    for (const LocalTriangle& triangle : triangles)
    {
        rasterise(triangle, grid);
    }

    std::vector<char> occupied(grid.width * grid.height, 0);
    std::vector<char> plugged(grid.width * grid.height, 0);
    std::vector<double> coverage(grid.width * grid.height, 0.0);

    double thickness = frame.front - frame.back;

    for (std::size_t k = 0; k < occupied.size(); ++k)
    {
        if (!grid.covered(k))
        {
            continue;
        }

        if (grid.minDepth[k] <= frame.front && grid.maxDepth[k] >= frame.back)
        {
            occupied[k] = 1;
            coverage[k] = std::min(grid.maxDepth[k], frame.front) -
                std::max(grid.minDepth[k], frame.back);
        }

        if (grid.maxDepth[k] >= frame.front - settings.faceTolerance &&
            grid.minDepth[k] <= frame.back + settings.faceTolerance)
        {
            plugged[k] = 1;
        }
    }

    std::vector<char> dropped;

    std::vector<char> hole = selectOpenings(occupied, coverage,
        settings.apertureFraction * thickness, grid.width, grid.height,
        grid.cell * grid.cell, settings.minOpeningArea, dropped, solution.ignoredParts);

    if (solution.ignoredParts > 0)
    {
        solution.ignored = gridToRings(dropped, grid);
    }

    fillBracketed(plugged, grid.width, grid.height);

    std::size_t holeCells = 0;
    std::size_t leakCells = 0;

    for (std::size_t k = 0; k < hole.size(); ++k)
    {
        if (!hole[k])
        {
            continue;
        }

        ++holeCells;

        if (!plugged[k])
        {
            ++leakCells;
        }
    }

    if (holeCells == 0)
    {
        return solution;
    }

    double cellArea = grid.cell * grid.cell;

    solution.leakArea = leakCells * cellArea;

    std::vector<polygon::Ring> rings = gridToRings(hole, grid);

    if (rings.empty())
    {
        return solution;
    }

    std::vector<Vector2> silhouette =
        collectSilhouetteVertices(triangles, hole, grid, frame.back, frame.front);
    double rasterArea = ringsArea(rings);
    double areaLimit = rasterArea * (1.0 + settings.maxGrowth);

    std::vector<polygon::Ring> pieces;
    std::vector<std::vector<polygon::Ring>> candidates;

    std::vector<polygon::Ring> hull = hullRings(rings);
    std::vector<polygon::Ring> snappedHull = snapRings(hull, silhouette, settings.snap);

    candidates.push_back(polygon::repair(snappedHull, settings.simplify));
    candidates.push_back(polygon::repair(hull, settings.simplify));
    candidates.push_back(polygon::repair(snappedHull, 0.0));
    candidates.push_back(polygon::repair(hull, 0.0));

    for (double epsilon : { settings.simplify, settings.simplify * 0.5,
        settings.simplify * 0.25, 0.0 })
    {
        candidates.push_back(polygon::repair(snapRings(rings, silhouette, settings.snap), epsilon));
    }

    for (const std::vector<polygon::Ring>& candidate : candidates)
    {
        if (candidate.empty() || ringsArea(candidate) > areaLimit)
        {
            continue;
        }

        if (!ringsCoverSlab(candidate, triangles, frame.back, frame.front, CONTAINMENT_TOLERANCE))
        {
            continue;
        }

        std::vector<polygon::Ring> decomposed = polygon::convexPieces(candidate, 0.0);

        if (decomposed.empty() ||
            !ringsCoverSlab(decomposed, triangles, frame.back, frame.front, grid.cell))
        {
            continue;
        }

        rings = candidate;
        pieces = decomposed;
        break;
    }

    if (pieces.empty())
    {
        pieces = polygon::convexPieces(rings, 0.0);
    }

    solution.outline = rings;
    solution.area = ringsArea(rings);

    for (const polygon::Ring& piece : pieces)
    {
        if (piece.size() < 3 || std::abs(polygon::ringArea(piece)) < settings.minPieceArea)
        {
            continue;
        }

        solution.pieces.push_back(piece);
    }

    solution.valid = !solution.pieces.empty();

    return solution;
}

} // namespace algorithm

} // namespace brush
