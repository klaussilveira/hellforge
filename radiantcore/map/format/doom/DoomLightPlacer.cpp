#include "DoomLightPlacer.h"

#include <algorithm>
#include <cmath>
#include <random>
#include <unordered_map>

namespace map
{

namespace doom
{

namespace
{

const int CANDIDATES_PER_POINT = 12;
const double MAX_LIGHT_HEIGHT = 64;

const int LEVEL_TOLERANCE = 32;

struct Candidate
{
	Vector2 position;
	double sectorArea = 0;
	int sector = NO_INDEX;
	int lightLevel = 160;
};

bool isInsideRing(const polygon::Ring& ring, const Vector2& point)
{
	double sign = polygon::ringArea(ring) >= 0 ? 1.0 : -1.0;

	for (std::size_t i = 0; i < ring.size(); ++i)
	{
		const auto& from = ring[i];
		const auto& to = ring[(i + 1) % ring.size()];

		if (sign * (to - from).crossProduct(point - from) < 0)
		{
			return false;
		}
	}

	return true;
}

bool isInsideFootprint(const SectorFootprint& footprint, const Vector2& point)
{
	for (const auto& piece : footprint.pieces)
	{
		if (isInsideRing(piece, point))
		{
			return true;
		}
	}

	return false;
}

Vector2 ringCentroid(const polygon::Ring& ring)
{
	Vector2 sum(0, 0);

	for (const auto& point : ring)
	{
		sum += point;
	}

	return sum / static_cast<double>(ring.size());
}

Vector2 seedPosition(const SectorFootprint& footprint, double& largestArea)
{
	const polygon::Ring* largest = nullptr;
	largestArea = -1;

	for (const auto& piece : footprint.pieces)
	{
		auto area = std::fabs(polygon::ringArea(piece));

		if (area > largestArea)
		{
			largestArea = area;
			largest = &piece;
		}
	}

	return largest ? ringCentroid(*largest) : Vector2(0, 0);
}

void sampleSector(const SectorFootprint& footprint, double spacing, std::mt19937& random,
	std::vector<Vector2>& result)
{
	double largestArea = 0;
	result.push_back(seedPosition(footprint, largestArea));

	if (largestArea < spacing * spacing)
	{
		return;
	}

	std::uniform_real_distribution<double> angleRange(0, 6.283185307179586);
	std::uniform_real_distribution<double> radiusRange(spacing, spacing * 2);

	std::vector<std::size_t> active{ 0 };

	auto spacingSquared = spacing * spacing;

	while (!active.empty())
	{
		std::uniform_int_distribution<std::size_t> pick(0, active.size() - 1);

		auto slot = pick(random);
		auto origin = result[active[slot]];

		bool found = false;

		for (int attempt = 0; attempt < CANDIDATES_PER_POINT; ++attempt)
		{
			auto angle = angleRange(random);
			auto distance = radiusRange(random);

			Vector2 candidate(origin.x() + std::cos(angle) * distance,
				origin.y() + std::sin(angle) * distance);

			if (!isInsideFootprint(footprint, candidate))
			{
				continue;
			}

			bool tooClose = false;

			for (const auto& existing : result)
			{
				if ((existing - candidate).getLengthSquared() < spacingSquared)
				{
					tooClose = true;
					break;
				}
			}

			if (tooClose)
			{
				continue;
			}

			result.push_back(candidate);
			active.push_back(result.size() - 1);
			found = true;
			break;
		}

		if (!found)
		{
			active.erase(active.begin() + slot);
		}
	}
}

class AcceptedGrid
{
	double _cellSize;
	std::unordered_map<long long, std::vector<std::size_t>> _cells;

	static long long key(long long column, long long row)
	{
		return column * 73856093LL ^ row * 19349663LL;
	}

public:
	AcceptedGrid(double cellSize) :
		_cellSize(cellSize)
	{}

	void insert(const Vector2& position, std::size_t index)
	{
		auto column = static_cast<long long>(std::floor(position.x() / _cellSize));
		auto row = static_cast<long long>(std::floor(position.y() / _cellSize));

		_cells[key(column, row)].push_back(index);
	}

	template<typename Predicate>
	bool anyNeighbour(const Vector2& position, const Predicate& predicate) const
	{
		auto column = static_cast<long long>(std::floor(position.x() / _cellSize));
		auto row = static_cast<long long>(std::floor(position.y() / _cellSize));

		for (long long dy = -1; dy <= 1; ++dy)
		{
			for (long long dx = -1; dx <= 1; ++dx)
			{
				auto cell = _cells.find(key(column + dx, row + dy));

				if (cell == _cells.end())
				{
					continue;
				}

				for (auto index : cell->second)
				{
					if (predicate(index))
					{
						return true;
					}
				}
			}
		}

		return false;
	}
};

}

std::vector<DoomLight> DoomLightPlacer::place(const DoomMapData& data,
	const std::vector<SectorFootprint>& footprints, double spacing)
{
	std::vector<DoomLight> lights;

	if (spacing <= 0)
	{
		return lights;
	}

	std::mt19937 random(20260903);

	std::vector<Candidate> candidates;

	for (std::size_t i = 0; i < footprints.size(); ++i)
	{
		const auto& footprint = footprints[i];
		const auto& sector = data.sectors[i];

		if (!footprint.valid || sector.ceilingHeight <= sector.floorHeight)
		{
			continue;
		}

		double area = 0;

		for (const auto& ring : footprint.rings)
		{
			area += polygon::ringArea(ring);
		}

		std::vector<Vector2> positions;
		sampleSector(footprint, spacing, random, positions);

		for (const auto& position : positions)
		{
			Candidate candidate;
			candidate.position = position;
			candidate.sectorArea = area;
			candidate.sector = static_cast<int>(i);
			candidate.lightLevel = sector.lightLevel;

			candidates.push_back(candidate);
		}
	}

	std::stable_sort(candidates.begin(), candidates.end(),
		[](const Candidate& a, const Candidate& b) { return a.sectorArea > b.sectorArea; });

	auto minRoomArea = spacing * spacing / 16;

	AcceptedGrid grid(spacing);

	auto spacingSquared = spacing * spacing;

	std::vector<bool> sectorIsLit(footprints.size(), false);

	for (const auto& candidate : candidates)
	{
		bool needsOwnLight = !sectorIsLit[candidate.sector] && candidate.sectorArea >= minRoomArea;

		if (!needsOwnLight)
		{
			bool covered = grid.anyNeighbour(candidate.position, [&](std::size_t index)
			{
				const auto& light = lights[index];

				return (light.position - candidate.position).getLengthSquared() < spacingSquared &&
					std::abs(light.lightLevel - candidate.lightLevel) <= LEVEL_TOLERANCE;
			});

			if (covered)
			{
				continue;
			}
		}

		const auto& sector = data.sectors[candidate.sector];

		DoomLight light;
		light.position = candidate.position;
		light.height = sector.floorHeight +
			std::min(MAX_LIGHT_HEIGHT, (sector.ceilingHeight - sector.floorHeight) * 0.5);
		light.radius = spacing;
		light.lightLevel = candidate.lightLevel;

		grid.insert(light.position, lights.size());
		lights.push_back(light);

		sectorIsLit[candidate.sector] = true;
	}

	return lights;
}

}

}
