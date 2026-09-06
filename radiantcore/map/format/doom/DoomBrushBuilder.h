#pragma once

#include "DoomMapData.h"
#include "SectorGeometry.h"

#include "inode.h"
#include "math/Matrix3.h"
#include "math/Vector2.h"
#include "math/Vector3.h"

#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

namespace map
{

namespace doom
{

const double FLAT_NOMINAL_SIZE = 64;
const double WALL_NOMINAL_SIZE = 128;

typedef std::function<void(const scene::INodePtr&)> BrushSink;

class DoomBrushBuilder
{
	struct ResolvedTexture
	{
		std::string material;
		double width = 128;
		double height = 128;
	};

	struct EdgeMatch
	{
		int lineIndex = NO_INDEX;
		double overlap = 0;
	};

	const DoomMapData& _data;
	const std::vector<SectorFootprint>& _footprints;
	double _scale;

	std::string _caulk;
	std::unordered_map<std::string, ResolvedTexture> _textures;

	std::vector<std::vector<int>> _sectorLines;

	double _gridOrigin[2] = { 0, 0 };
	double _gridCellSize = 128;
	std::size_t _gridColumns = 0;
	std::size_t _gridRows = 0;
	std::vector<std::vector<int>> _grid;

public:
	DoomBrushBuilder(const DoomMapData& data, const std::vector<SectorFootprint>& footprints,
		double scale);

	void buildSectorBrushes(const BrushSink& sink);

	void buildShellBrushes(const std::vector<polygon::Ring>& shell, double thickness,
		const BrushSink& sink);

private:
	const ResolvedTexture& resolveTexture(const std::string& rawName);

	void buildSectorLines();
	void buildLineGrid();

	std::vector<int> queryGrid(const Vector2& min, const Vector2& max) const;

	EdgeMatch matchEdge(const Vector2& from, const Vector2& to,
		const std::vector<int>& candidates) const;

	scene::INodePtr createPrism(const polygon::Ring& ring, double bottom, double top,
		int sectorIndex, bool isFloorPrism);

	scene::INodePtr createShellPiece(const polygon::Ring& ring, double bottom, double top);

	Matrix3 wallProjection(const ResolvedTexture& texture, const Vector3& normal,
		int lineIndex, bool useFrontSide, double pegHeight) const;

	Matrix3 flatProjection(const ResolvedTexture& texture, const Vector3& normal) const;
};

}

}
