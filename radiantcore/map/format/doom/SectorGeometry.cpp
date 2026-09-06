#include "SectorGeometry.h"

#include "itextstream.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <functional>
#include <thread>
#include <unordered_map>

namespace map
{

namespace doom
{

namespace
{

const double TWO_PI = 6.283185307179586;

const double CONTROL_SECTOR_DISTANCE = 4096;

struct SectorEdge
{
	int from = NO_INDEX;
	int to = NO_INDEX;
	bool used = false;
};

struct SectorEdges
{
	std::vector<SectorEdge> edges;
	std::unordered_map<int, std::vector<std::size_t>> outgoing;
};

Vector2 vertexPosition(const DoomMapData& data, int index)
{
	const auto& vertex = data.vertices[index];
	return Vector2(vertex.x, vertex.y);
}

double clockwiseAngle(const Vector2& back, const Vector2& dir)
{
	double angle = std::atan2(back.crossProduct(dir), back.dot(dir));

	double clockwise = -angle;

	if (clockwise <= 1e-9)
	{
		clockwise += TWO_PI;
	}

	return clockwise;
}

bool traceRings(const DoomMapData& data, SectorEdges& sectorEdges, std::vector<polygon::Ring>& rings)
{
	for (std::size_t start = 0; start < sectorEdges.edges.size(); ++start)
	{
		if (sectorEdges.edges[start].used)
		{
			continue;
		}

		polygon::Ring ring;

		std::size_t current = start;
		std::size_t steps = 0;

		while (steps++ <= sectorEdges.edges.size())
		{
			auto& edge = sectorEdges.edges[current];

			if (edge.used)
			{
				break;
			}

			edge.used = true;
			ring.push_back(vertexPosition(data, edge.from));

			auto candidates = sectorEdges.outgoing.find(edge.to);

			if (candidates == sectorEdges.outgoing.end() || candidates->second.empty())
			{
				return false;
			}

			auto position = vertexPosition(data, edge.to);
			auto back = vertexPosition(data, edge.from) - position;

			std::size_t best = 0;
			double bestAngle = 0;
			bool found = false;

			for (auto candidate : candidates->second)
			{
				if (sectorEdges.edges[candidate].used && candidate != start)
				{
					continue;
				}

				auto dir = vertexPosition(data, sectorEdges.edges[candidate].to) - position;

				if (dir.getLengthSquared() < 1e-12)
				{
					continue;
				}

				double angle = clockwiseAngle(back, dir);

				if (!found || angle < bestAngle)
				{
					best = candidate;
					bestAngle = angle;
					found = true;
				}
			}

			if (!found)
			{
				return false;
			}

			if (best == start)
			{
				break;
			}

			current = best;
		}

		if (ring.size() >= 3)
		{
			rings.push_back(ring);
		}
	}

	return true;
}

void buildSectorEdges(const DoomMapData& data, std::vector<SectorEdges>& perSector)
{
	for (const auto& line : data.lineDefs)
	{
		if (line.v1 < 0 || line.v2 < 0 ||
			static_cast<std::size_t>(line.v1) >= data.vertices.size() ||
			static_cast<std::size_t>(line.v2) >= data.vertices.size() ||
			line.v1 == line.v2)
		{
			continue;
		}

		auto front = data.getSectorOfSide(line.sideFront);

		if (front != NO_INDEX)
		{
			perSector[front].edges.push_back(SectorEdge{ line.v2, line.v1 });
		}

		auto back = data.getSectorOfSide(line.sideBack);

		if (back != NO_INDEX)
		{
			perSector[back].edges.push_back(SectorEdge{ line.v1, line.v2 });
		}
	}

	for (auto& sector : perSector)
	{
		for (std::size_t i = 0; i < sector.edges.size(); ++i)
		{
			sector.outgoing[sector.edges[i].from].push_back(i);
		}
	}
}

std::vector<bool> findControlSectors(const DoomMapData& data)
{
	std::vector<bool> isControl(data.sectors.size(), false);

	if (data.sectors.empty())
	{
		return isControl;
	}

	std::vector<double> floors;
	floors.reserve(data.sectors.size());

	for (const auto& sector : data.sectors)
	{
		floors.push_back(sector.floorHeight);
	}

	auto middle = floors.begin() + floors.size() / 2;
	std::nth_element(floors.begin(), middle, floors.end());

	auto reference = *middle;

	for (std::size_t i = 0; i < data.sectors.size(); ++i)
	{
		const auto& sector = data.sectors[i];

		isControl[i] =
			std::fabs(sector.floorHeight - reference) > CONTROL_SECTOR_DISTANCE ||
			std::fabs(sector.ceilingHeight - reference) > CONTROL_SECTOR_DISTANCE;
	}

	return isControl;
}

void computePrismExtents(const DoomMapData& data, std::vector<SectorFootprint>& footprints,
	const std::vector<bool>& isControl, double margin)
{
	for (std::size_t i = 0; i < data.sectors.size(); ++i)
	{
		footprints[i].prismBottom = data.sectors[i].floorHeight;
		footprints[i].prismTop = data.sectors[i].ceilingHeight;
	}

	for (const auto& line : data.lineDefs)
	{
		auto front = data.getSectorOfSide(line.sideFront);
		auto back = data.getSectorOfSide(line.sideBack);

		if (front == NO_INDEX || back == NO_INDEX)
		{
			continue;
		}

		for (auto pair : { std::make_pair(front, back), std::make_pair(back, front) })
		{
			const auto& neighbour = data.sectors[pair.second];

			if (neighbour.ceilingHeight <= neighbour.floorHeight || isControl[pair.second])
			{
				continue;
			}

			auto& extents = footprints[pair.first];

			extents.prismBottom = std::min(extents.prismBottom, neighbour.floorHeight);
			extents.prismTop = std::max(extents.prismTop, neighbour.ceilingHeight);
		}
	}

	for (std::size_t i = 0; i < data.sectors.size(); ++i)
	{
		footprints[i].prismBottom -= margin;
		footprints[i].prismTop += margin;
	}
}

void parallelFor(std::size_t count, const std::function<void(std::size_t)>& task)
{
	auto cores = std::max(1u, std::thread::hardware_concurrency());
	auto threadCount = static_cast<std::size_t>(std::min<std::size_t>(cores, count));

	if (threadCount <= 1)
	{
		for (std::size_t i = 0; i < count; ++i)
		{
			task(i);
		}

		return;
	}

	std::atomic<std::size_t> next(0);
	std::vector<std::thread> threads;

	threads.reserve(threadCount);

	for (std::size_t t = 0; t < threadCount; ++t)
	{
		threads.emplace_back([&]()
		{
			for (auto i = next++; i < count; i = next++)
			{
				task(i);
			}
		});
	}

	for (auto& thread : threads)
	{
		thread.join();
	}
}

}

std::vector<SectorFootprint> SectorGeometry::build(const DoomMapData& data, double prismMargin)
{
	std::vector<SectorFootprint> footprints(data.sectors.size());
	std::vector<SectorEdges> perSector(data.sectors.size());

	auto isControl = findControlSectors(data);

	buildSectorEdges(data, perSector);
	computePrismExtents(data, footprints, isControl, prismMargin);

	std::atomic<std::size_t> brokenSectors(0);
	std::atomic<std::size_t> controlSectors(0);

	parallelFor(data.sectors.size(), [&](std::size_t index)
	{
		auto& footprint = footprints[index];

		if (isControl[index])
		{
			controlSectors++;
			return;
		}

		if (!traceRings(data, perSector[index], footprint.rings) || footprint.rings.empty())
		{
			brokenSectors++;
			footprint.rings.clear();
			return;
		}

		footprint.pieces = polygon::convexPieces(footprint.rings, 0);
		footprint.valid = !footprint.pieces.empty();

		if (!footprint.valid)
		{
			brokenSectors++;
			return;
		}

		footprint.boundsMin = footprint.pieces.front().front();
		footprint.boundsMax = footprint.boundsMin;

		for (const auto& piece : footprint.pieces)
		{
			for (const auto& point : piece)
			{
				footprint.boundsMin = Vector2(std::min(footprint.boundsMin.x(), point.x()),
					std::min(footprint.boundsMin.y(), point.y()));
				footprint.boundsMax = Vector2(std::max(footprint.boundsMax.x(), point.x()),
					std::max(footprint.boundsMax.y(), point.y()));
			}
		}
	});

	if (controlSectors > 0)
	{
		rMessage() << "[wad]: skipped " << controlSectors
			<< " control sectors parked outside the map" << std::endl;
	}

	if (brokenSectors > 0)
	{
		rWarning() << "[wad]: " << brokenSectors << " of " << data.sectors.size()
			<< " sectors could not be converted into a closed polygon and were skipped."
			<< std::endl;
	}

	return footprints;
}

std::vector<polygon::Ring> SectorGeometry::buildShell(const std::vector<SectorFootprint>& footprints,
	double thickness)
{
	std::vector<polygon::Ring> all;

	for (const auto& footprint : footprints)
	{
		if (!footprint.valid)
		{
			continue;
		}

		all.insert(all.end(), footprint.rings.begin(), footprint.rings.end());
	}

	if (all.empty())
	{
		return {};
	}

	auto footprint = polygon::repair(all, 0);

	auto inflated = polygon::toRings(Clipper2Lib::InflatePaths(polygon::toPaths(footprint),
		thickness, Clipper2Lib::JoinType::Miter, Clipper2Lib::EndType::Polygon,
		polygon::MITER_LIMIT, polygon::PRECISION));

	auto shell = polygon::difference(inflated, footprint);

	return polygon::convexPieces(shell, 0);
}

}

}
