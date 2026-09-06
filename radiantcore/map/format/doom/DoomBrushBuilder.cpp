#include "DoomBrushBuilder.h"

#include "../ConversionMap.h"

#include "ibrush.h"
#include "ishaders.h"
#include "gamelib.h"
#include "shaderlib.h"
#include "texturelib.h"
#include "math/Matrix4.h"
#include "math/Plane3.h"

#include <algorithm>
#include <cmath>

namespace map
{

namespace doom
{

namespace
{

const double COLLINEAR_TOLERANCE = 0.5;
const double MIN_OVERLAP = 0.5;

Matrix3 buildTextureMatrix(const Vector3& normal, const Vector3& uAxis, const Vector3& vAxis,
	double shiftU, double shiftV, double scaleU, double scaleV,
	double imageWidth, double imageHeight)
{
	auto transform = Matrix4::getIdentity();

	transform.xx() = uAxis.x() / scaleU / imageWidth;
	transform.yx() = uAxis.y() / scaleU / imageWidth;
	transform.zx() = uAxis.z() / scaleU / imageWidth;

	transform.xy() = vAxis.x() / scaleV / imageHeight;
	transform.yy() = vAxis.y() / scaleV / imageHeight;
	transform.zy() = vAxis.z() / scaleV / imageHeight;

	transform.tx() = shiftU / imageWidth;
	transform.ty() = shiftV / imageHeight;

	auto axisBase = getBasisTransformForNormal(normal);
	transform.multiplyBy(axisBase.getTransposed());

	return getTextureMatrixFromMatrix4(transform);
}

}

DoomBrushBuilder::DoomBrushBuilder(const DoomMapData& data,
	const std::vector<SectorFootprint>& footprints, double scale) :
	_data(data),
	_footprints(footprints),
	_scale(scale)
{
	_caulk = game::current::getValue<std::string>("/defaults/nodrawShader");

	if (_caulk.empty())
	{
		_caulk = GlobalTexturePrefix_get() + texdef_name_default();
	}

	buildSectorLines();
	buildLineGrid();
}

const DoomBrushBuilder::ResolvedTexture& DoomBrushBuilder::resolveTexture(const std::string& rawName)
{
	auto existing = _textures.find(rawName);

	if (existing != _textures.end())
	{
		return existing->second;
	}

	ResolvedTexture resolved;

	const auto& mapped = ConversionMap::lookup(rawName);

	if (!mapped.empty())
	{
		resolved.material = mapped;
	}
	else
	{
		std::string prefix = GlobalTexturePrefix_get();

		if (GlobalMaterialManager().materialExists(prefix + rawName))
		{
			resolved.material = prefix + rawName;
		}
		else if (GlobalMaterialManager().materialExists(rawName))
		{
			resolved.material = rawName;
		}
		else
		{
			resolved.material = prefix + texdef_name_default();
		}
	}

	auto material = GlobalMaterialManager().getMaterial(resolved.material);
	auto image = material ? material->getEditorImage() : TexturePtr();

	if (image && image->getWidth() > 0 && image->getHeight() > 0)
	{
		resolved.width = static_cast<double>(image->getWidth());
		resolved.height = static_cast<double>(image->getHeight());
	}

	return _textures.emplace(rawName, resolved).first->second;
}

void DoomBrushBuilder::buildSectorLines()
{
	_sectorLines.resize(_data.sectors.size());

	for (std::size_t i = 0; i < _data.lineDefs.size(); ++i)
	{
		const auto& line = _data.lineDefs[i];

		auto front = _data.getSectorOfSide(line.sideFront);
		auto back = _data.getSectorOfSide(line.sideBack);

		if (front != NO_INDEX)
		{
			_sectorLines[front].push_back(static_cast<int>(i));
		}

		if (back != NO_INDEX && back != front)
		{
			_sectorLines[back].push_back(static_cast<int>(i));
		}
	}
}

void DoomBrushBuilder::buildLineGrid()
{
	if (_data.vertices.empty())
	{
		return;
	}

	double minX = _data.vertices[0].x;
	double maxX = minX;
	double minY = _data.vertices[0].y;
	double maxY = minY;

	for (const auto& vertex : _data.vertices)
	{
		minX = std::min(minX, vertex.x);
		maxX = std::max(maxX, vertex.x);
		minY = std::min(minY, vertex.y);
		maxY = std::max(maxY, vertex.y);
	}

	_gridOrigin[0] = minX;
	_gridOrigin[1] = minY;

	_gridColumns = static_cast<std::size_t>((maxX - minX) / _gridCellSize) + 1;
	_gridRows = static_cast<std::size_t>((maxY - minY) / _gridCellSize) + 1;

	_grid.resize(_gridColumns * _gridRows);

	for (std::size_t i = 0; i < _data.lineDefs.size(); ++i)
	{
		const auto& line = _data.lineDefs[i];

		if (line.sideBack != NO_INDEX || line.v1 < 0 || line.v2 < 0)
		{
			continue;
		}

		const auto& v1 = _data.vertices[line.v1];
		const auto& v2 = _data.vertices[line.v2];

		auto firstColumn = static_cast<std::size_t>((std::min(v1.x, v2.x) - minX) / _gridCellSize);
		auto lastColumn = static_cast<std::size_t>((std::max(v1.x, v2.x) - minX) / _gridCellSize);
		auto firstRow = static_cast<std::size_t>((std::min(v1.y, v2.y) - minY) / _gridCellSize);
		auto lastRow = static_cast<std::size_t>((std::max(v1.y, v2.y) - minY) / _gridCellSize);

		for (auto row = firstRow; row <= lastRow && row < _gridRows; ++row)
		{
			for (auto column = firstColumn; column <= lastColumn && column < _gridColumns; ++column)
			{
				_grid[row * _gridColumns + column].push_back(static_cast<int>(i));
			}
		}
	}
}

std::vector<int> DoomBrushBuilder::queryGrid(const Vector2& min, const Vector2& max) const
{
	std::vector<int> result;

	if (_grid.empty())
	{
		return result;
	}

	auto clampColumn = [this](double x)
	{
		auto value = (x - _gridOrigin[0]) / _gridCellSize;
		return static_cast<std::size_t>(std::min<double>(std::max(value, 0.0),
			static_cast<double>(_gridColumns - 1)));
	};

	auto clampRow = [this](double y)
	{
		auto value = (y - _gridOrigin[1]) / _gridCellSize;
		return static_cast<std::size_t>(std::min<double>(std::max(value, 0.0),
			static_cast<double>(_gridRows - 1)));
	};

	auto firstColumn = clampColumn(min.x());
	auto lastColumn = clampColumn(max.x());
	auto firstRow = clampRow(min.y());
	auto lastRow = clampRow(max.y());

	for (auto row = firstRow; row <= lastRow; ++row)
	{
		for (auto column = firstColumn; column <= lastColumn; ++column)
		{
			const auto& cell = _grid[row * _gridColumns + column];
			result.insert(result.end(), cell.begin(), cell.end());
		}
	}

	std::sort(result.begin(), result.end());
	result.erase(std::unique(result.begin(), result.end()), result.end());

	return result;
}

DoomBrushBuilder::EdgeMatch DoomBrushBuilder::matchEdge(const Vector2& from, const Vector2& to,
	const std::vector<int>& candidates) const
{
	EdgeMatch best;

	for (auto index : candidates)
	{
		const auto& line = _data.lineDefs[index];

		Vector2 v1(_data.vertices[line.v1].x, _data.vertices[line.v1].y);
		Vector2 v2(_data.vertices[line.v2].x, _data.vertices[line.v2].y);

		auto span = v2 - v1;
		auto length = span.getLength();

		if (length < 1e-6)
		{
			continue;
		}

		auto direction = span / length;

		if (std::fabs(direction.crossProduct(from - v1)) > COLLINEAR_TOLERANCE ||
			std::fabs(direction.crossProduct(to - v1)) > COLLINEAR_TOLERANCE)
		{
			continue;
		}

		auto tFrom = direction.dot(from - v1);
		auto tTo = direction.dot(to - v1);

		auto overlap = std::min(std::max(tFrom, tTo), length) - std::max(std::min(tFrom, tTo), 0.0);

		if (overlap > best.overlap)
		{
			best.lineIndex = index;
			best.overlap = overlap;
		}
	}

	if (best.overlap < MIN_OVERLAP)
	{
		return EdgeMatch();
	}

	return best;
}

Matrix3 DoomBrushBuilder::wallProjection(const ResolvedTexture& texture, const Vector3& normal,
	int lineIndex, bool useFrontSide, double pegHeight) const
{
	const auto& line = _data.lineDefs[lineIndex];
	const auto& side = _data.sideDefs[useFrontSide ? line.sideFront : line.sideBack];

	const auto& startVertex = useFrontSide ? _data.vertices[line.v1] : _data.vertices[line.v2];
	const auto& endVertex = useFrontSide ? _data.vertices[line.v2] : _data.vertices[line.v1];

	Vector3 origin(startVertex.x * _scale, startVertex.y * _scale, 0);
	Vector3 uAxis(endVertex.x * _scale - origin.x(), endVertex.y * _scale - origin.y(), 0);

	auto length = uAxis.getLength();

	if (length > 1e-6)
	{
		uAxis /= length;
	}
	else
	{
		uAxis = Vector3(1, 0, 0);
	}

	Vector3 vAxis(0, 0, -1);

	auto scaleU = WALL_NOMINAL_SIZE * _scale / texture.width;
	auto scaleV = WALL_NOMINAL_SIZE * _scale / texture.height;

	auto shiftU = side.offsetX * texture.width / WALL_NOMINAL_SIZE - origin.dot(uAxis) / scaleU;
	auto shiftV = side.offsetY * texture.height / WALL_NOMINAL_SIZE + pegHeight * _scale / scaleV;

	return buildTextureMatrix(normal, uAxis, vAxis, shiftU, shiftV, scaleU, scaleV,
		texture.width, texture.height);
}

Matrix3 DoomBrushBuilder::flatProjection(const ResolvedTexture& texture, const Vector3& normal) const
{
	auto scaleU = FLAT_NOMINAL_SIZE * _scale / texture.width;
	auto scaleV = FLAT_NOMINAL_SIZE * _scale / texture.height;

	return buildTextureMatrix(normal, Vector3(1, 0, 0), Vector3(0, -1, 0), 0, 0,
		scaleU, scaleV, texture.width, texture.height);
}

scene::INodePtr DoomBrushBuilder::createPrism(const polygon::Ring& ring, double bottom, double top,
	int sectorIndex, bool isFloorPrism)
{
	if (ring.size() < 3 || top - bottom < 1e-3)
	{
		return scene::INodePtr();
	}

	const auto& sector = _data.sectors[sectorIndex];

	auto node = GlobalBrushCreator().createBrush();
	auto* brush = Node_getIBrush(node);

	if (!brush)
	{
		return scene::INodePtr();
	}

	brush->clear();

	{
		Vector3 normal(0, 0, 1);
		Plane3 plane(normal, top * _scale);

		if (isFloorPrism)
		{
			const auto& texture = resolveTexture(sector.floorTexture);
			brush->addFace(plane, flatProjection(texture, normal), texture.material);
		}
		else
		{
			brush->addFace(plane).setShader(_caulk);
		}
	}

	{
		Vector3 normal(0, 0, -1);
		Plane3 plane(normal, -bottom * _scale);

		if (isFloorPrism)
		{
			brush->addFace(plane).setShader(_caulk);
		}
		else
		{
			const auto& texture = resolveTexture(sector.ceilingTexture);
			brush->addFace(plane, flatProjection(texture, normal), texture.material);
		}
	}

	double sign = polygon::ringArea(ring) >= 0 ? 1.0 : -1.0;

	for (std::size_t i = 0; i < ring.size(); ++i)
	{
		const auto& from = ring[i];
		const auto& to = ring[(i + 1) % ring.size()];

		auto span = to - from;
		auto length = span.getLength();

		if (length < 1e-6)
		{
			continue;
		}

		Vector3 normal(sign * span.y() / length, -sign * span.x() / length, 0);
		Plane3 plane(normal, normal.x() * from.x() * _scale + normal.y() * from.y() * _scale);

		auto match = matchEdge(from, to, _sectorLines[sectorIndex]);

		if (match.lineIndex == NO_INDEX)
		{
			brush->addFace(plane).setShader(_caulk);
			continue;
		}

		const auto& line = _data.lineDefs[match.lineIndex];

		auto front = _data.getSectorOfSide(line.sideFront);
		auto back = _data.getSectorOfSide(line.sideBack);

		bool weAreFront = front == sectorIndex;
		auto neighbour = weAreFront ? back : front;

		if (neighbour == NO_INDEX || neighbour == sectorIndex)
		{
			brush->addFace(plane).setShader(_caulk);
			continue;
		}

		bool useFrontSide = !weAreFront;
		auto sideIndex = useFrontSide ? line.sideFront : line.sideBack;

		if (sideIndex == NO_INDEX)
		{
			brush->addFace(plane).setShader(_caulk);
			continue;
		}

		const auto& side = _data.sideDefs[sideIndex];
		const auto& neighbourSector = _data.sectors[neighbour];

		std::string rawName;
		double pegHeight = 0;

		if (isFloorPrism)
		{
			rawName = side.lowerTexture;

			pegHeight = line.dontPegBottom ? neighbourSector.ceilingHeight : sector.floorHeight;
		}
		else
		{
			rawName = side.upperTexture;

			pegHeight = line.dontPegTop ?
				neighbourSector.ceilingHeight : sector.ceilingHeight + WALL_NOMINAL_SIZE;
		}

		if (!textureIsSet(rawName))
		{
			brush->addFace(plane).setShader(_caulk);
			continue;
		}

		const auto& texture = resolveTexture(rawName);

		brush->addFace(plane, wallProjection(texture, normal, match.lineIndex, useFrontSide, pegHeight),
			texture.material);
	}

	brush->removeRedundantFaces();

	brush->evaluateBRep();

	return brush->hasContributingFaces() ? node : scene::INodePtr();
}

scene::INodePtr DoomBrushBuilder::createShellPiece(const polygon::Ring& ring, double bottom, double top)
{
	if (ring.size() < 3 || top - bottom < 1e-3)
	{
		return scene::INodePtr();
	}

	auto node = GlobalBrushCreator().createBrush();
	auto* brush = Node_getIBrush(node);

	if (!brush)
	{
		return scene::INodePtr();
	}

	brush->clear();

	brush->addFace(Plane3(Vector3(0, 0, 1), top * _scale)).setShader(_caulk);
	brush->addFace(Plane3(Vector3(0, 0, -1), -bottom * _scale)).setShader(_caulk);

	double sign = polygon::ringArea(ring) >= 0 ? 1.0 : -1.0;

	Vector2 min = ring[0];
	Vector2 max = ring[0];

	for (const auto& point : ring)
	{
		min = Vector2(std::min(min.x(), point.x()), std::min(min.y(), point.y()));
		max = Vector2(std::max(max.x(), point.x()), std::max(max.y(), point.y()));
	}

	auto candidates = queryGrid(min, max);

	for (std::size_t i = 0; i < ring.size(); ++i)
	{
		const auto& from = ring[i];
		const auto& to = ring[(i + 1) % ring.size()];

		auto span = to - from;
		auto length = span.getLength();

		if (length < 1e-6)
		{
			continue;
		}

		Vector3 normal(sign * span.y() / length, -sign * span.x() / length, 0);
		Plane3 plane(normal, normal.x() * from.x() * _scale + normal.y() * from.y() * _scale);

		auto match = matchEdge(from, to, candidates);

		if (match.lineIndex == NO_INDEX)
		{
			brush->addFace(plane).setShader(_caulk);
			continue;
		}

		const auto& line = _data.lineDefs[match.lineIndex];
		auto front = _data.getSectorOfSide(line.sideFront);

		if (front == NO_INDEX || line.sideFront == NO_INDEX)
		{
			brush->addFace(plane).setShader(_caulk);
			continue;
		}

		const auto& side = _data.sideDefs[line.sideFront];

		if (!textureIsSet(side.middleTexture))
		{
			brush->addFace(plane).setShader(_caulk);
			continue;
		}

		const auto& sector = _data.sectors[front];

		auto pegHeight = line.dontPegBottom ?
			sector.floorHeight + WALL_NOMINAL_SIZE : sector.ceilingHeight;

		const auto& texture = resolveTexture(side.middleTexture);

		brush->addFace(plane, wallProjection(texture, normal, match.lineIndex, true, pegHeight),
			texture.material);
	}

	brush->removeRedundantFaces();

	brush->evaluateBRep();

	return brush->hasContributingFaces() ? node : scene::INodePtr();
}

void DoomBrushBuilder::buildSectorBrushes(const BrushSink& sink)
{
	for (std::size_t i = 0; i < _footprints.size(); ++i)
	{
		const auto& footprint = _footprints[i];

		if (!footprint.valid)
		{
			continue;
		}

		const auto& sector = _data.sectors[i];

		for (const auto& piece : footprint.pieces)
		{
			if (auto brush = createPrism(piece, footprint.prismBottom, sector.floorHeight,
				static_cast<int>(i), true); brush)
			{
				sink(brush);
			}

			if (auto brush = createPrism(piece, sector.ceilingHeight, footprint.prismTop,
				static_cast<int>(i), false); brush)
			{
				sink(brush);
			}
		}
	}
}

void DoomBrushBuilder::buildShellBrushes(const std::vector<polygon::Ring>& shell, double thickness,
	const BrushSink& sink)
{
	if (shell.empty() || _footprints.empty())
	{
		return;
	}

	double globalBottom = 0;
	double globalTop = 0;
	bool haveGlobal = false;

	for (const auto& footprint : _footprints)
	{
		if (!footprint.valid) continue;

		if (!haveGlobal)
		{
			globalBottom = footprint.prismBottom;
			globalTop = footprint.prismTop;
			haveGlobal = true;
		}
		else
		{
			globalBottom = std::min(globalBottom, footprint.prismBottom);
			globalTop = std::max(globalTop, footprint.prismTop);
		}
	}

	for (const auto& piece : shell)
	{
		if (piece.size() < 3)
		{
			continue;
		}

		Vector2 min = piece[0];
		Vector2 max = piece[0];

		for (const auto& point : piece)
		{
			min = Vector2(std::min(min.x(), point.x()), std::min(min.y(), point.y()));
			max = Vector2(std::max(max.x(), point.x()), std::max(max.y(), point.y()));
		}

		Vector2 margin(thickness * 2, thickness * 2);
		auto neighbours = queryGrid(min - margin, max + margin);

		double bottom = globalBottom;
		double top = globalTop;
		bool haveLocal = false;

		for (auto index : neighbours)
		{
			auto sectorIndex = _data.getSectorOfSide(_data.lineDefs[index].sideFront);

			if (sectorIndex == NO_INDEX || !_footprints[sectorIndex].valid)
			{
				continue;
			}

			const auto& footprint = _footprints[sectorIndex];

			if (!haveLocal)
			{
				bottom = footprint.prismBottom;
				top = footprint.prismTop;
				haveLocal = true;
			}
			else
			{
				bottom = std::min(bottom, footprint.prismBottom);
				top = std::max(top, footprint.prismTop);
			}
		}

		if (auto brush = createShellPiece(piece, bottom, top); brush)
		{
			sink(brush);
		}
	}
}

}

}
