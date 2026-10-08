#include "ValveVmfFormat.h"

#include "Quake3MapWriter.h"
#include "ConversionMap.h"
#include "valve/VmfLoader.h"
#include "primitiveparsers/Quake1BrushDef.h"
#include "module/StaticModule.h"
#include "imap.h"
#include "ipatch.h"
#include "itextstream.h"

#include <fmt/format.h>

#include <algorithm>
#include <iterator>

namespace map
{

using namespace goldsrc;

namespace
{

const std::size_t DISPLACEMENT_SUBDIVISIONS = 2;

const std::set<std::string> NON_SOLID_CLASSES = {
	"func_illusionary", "func_dustmotes", "func_dustcloud", "func_smokevolume", "func_precipitation",
	"fog_volume", "color_correction_volume", "func_buyzone", "func_bomb_target", "func_hostage_rescue", "func_nobuild"
};

const char* const SOURCE_LIGHT_KEYS[] = {
	"_lightHDR", "_lightscaleHDR", "_constant_attn", "_linear_attn", "_quadratic_attn", "_fifty_percent_distance",
	"_zero_percent_distance", "_distance", "_inner_cone", "_exponent", "_hardfalloff", "_ambient", "_ambientHDR",
	"_AmbientScaleHDR"
};

double lightRadius(Entity& entity)
{
	for (const char* key : { "_zero_percent_distance", "_distance" })
	{
		auto distance = parseNumbers(findValue(entity, key));

		if (!distance.empty() && distance[0] > 0)
		{
			return distance[0];
		}
	}

	auto half = parseNumbers(findValue(entity, "_fifty_percent_distance"));

	return !half.empty() && half[0] > 0 ? half[0] * 2 : 0;
}

void convertSourceLight(Entity& entity, const AABB& worldBounds, double scale)
{
	if (!findValue(entity, "_light"))
	{
		return;
	}

	convertLight(entity, worldBounds, scale, "_cone", lightRadius(entity));

	for (const char* key : SOURCE_LIGHT_KEYS)
	{
		removeKey(entity, key);
	}
}

void convertSolidity(Entity& entity)
{
	auto solidity = parseNumbers(findValue(entity, "Solidity"));
	bool nonSolid = NON_SOLID_CLASSES.count(entity.keyValues.front().second) > 0 || (!solidity.empty() && solidity[0] == 1);

	removeKey(entity, "Solidity");

	if (nonSolid && !entity.solids.empty())
	{
		setValue(entity, "solid", "0");
	}
}

void convertMoveDirection(Entity& entity)
{
	auto moveDirection = parseNumbers(findValue(entity, "movedir"));

	if (entity.solids.empty() || moveDirection.size() != 3)
	{
		convertAngles(entity, !entity.solids.empty());
		return;
	}

	double direction = moveDirection[0] == -90 ? -1 : (moveDirection[0] == 90 ? -2 : moveDirection[1]);

	setValue(entity, "movedir", fmt::format("{0:g}", direction));
	removeKey(entity, "angles");
}

void passThrough(PatchControl& control, const PatchControl& before, const PatchControl& after)
{
	control.vertex = control.vertex * 2 - (before.vertex + after.vertex) * 0.5;
	control.texcoord = control.texcoord * 2 - (before.texcoord + after.texcoord) * 0.5;
}

scene::INodePtr createDisplacement(const Face& face, const Vector3& origin)
{
	const auto& displacement = *face.displacement;
	const auto* corners = displacement.corners;

	auto size = (std::size_t(1) << displacement.power) + 1;
	auto scale = ConversionMap::getScale();
	auto shader = resolveTextureName(face.texture);

	auto [imageWidth, imageHeight] = textureSize(face, shader);

	std::vector<PatchControl> grid(size * size);

	for (std::size_t row = 0; row < size; ++row)
	{
		auto t = static_cast<double>(row) / (size - 1);
		auto start = corners[0] + (corners[1] - corners[0]) * t;
		auto end = corners[3] + (corners[2] - corners[3]) * t;

		for (std::size_t col = 0; col < size; ++col)
		{
			auto index = row * size + col;
			auto flat = start + (end - start) * (static_cast<double>(col) / (size - 1));

			auto point = flat + displacement.normals[index] * displacement.distances[index] +
				displacement.offsets[index] + face.plane.normal() * displacement.elevation;

			grid[index].vertex = (point - origin) * scale;
			grid[index].texcoord = Vector2((flat.dot(face.uAxis) / face.scaleU + face.shiftU) / imageWidth,
				(flat.dot(face.vAxis) / face.scaleV + face.shiftV) / imageHeight);
		}
	}

	for (std::size_t row = 0; row < size; ++row)
	{
		for (std::size_t col = 1; col < size; col += 2)
		{
			passThrough(grid[row * size + col], grid[row * size + col - 1], grid[row * size + col + 1]);
		}
	}

	for (std::size_t col = 0; col < size; ++col)
	{
		for (std::size_t row = 1; row < size; row += 2)
		{
			passThrough(grid[row * size + col], grid[(row - 1) * size + col], grid[(row + 1) * size + col]);
		}
	}

	auto node = GlobalPatchModule().createPatch(patch::PatchDefType::Def3);
	auto& patch = std::dynamic_pointer_cast<IPatchNode>(node)->getPatch();

	patch.setDims(size, size);
	patch.setFixedSubdivisions(true, Subdivisions(DISPLACEMENT_SUBDIVISIONS, DISPLACEMENT_SUBDIVISIONS));
	patch.setShader(shader);

	for (std::size_t row = 0; row < size; ++row)
	{
		for (std::size_t col = 0; col < size; ++col)
		{
			patch.ctrlAt(row, col) = grid[row * size + col];
		}
	}

	patch.controlPointsChanged();

	return node;
}

}

ValveVmfReader::ValveVmfReader(IMapImportFilter& importFilter) :
	_importFilter(importFilter)
{}

void ValveVmfReader::readFromStream(std::istream& stream)
{
	std::string contents{ std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>() };

	vmf::Statistics statistics;
	auto data = vmf::load(contents, ConversionMap::getMapPath(), statistics);

	if (statistics.hiddenObjects > 0)
	{
		rMessage() << fmt::format("[vmf]: Skipped {0} hidden objects", statistics.hiddenObjects) << std::endl;
	}

	if (statistics.connections > 0)
	{
		rMessage() << fmt::format("[vmf]: Dropped {0} entity connections, I/O conversion is not supported", statistics.connections) << std::endl;
	}

	if (statistics.instances > 0)
	{
		rMessage() << fmt::format("[vmf]: Merged {0} instances", statistics.instances - statistics.failedInstances.size()) << std::endl;
	}

	for (const auto& failed : statistics.failedInstances)
	{
		rWarning() << "[vmf]: Could not load instance " << failed << std::endl;
	}

	SceneAssigner assigner(_importFilter.getRootNode(), data.visgroups);

	auto dropped = removeToolSolids(data.world.solids, { "tools/toolshint", "tools/toolsskip" }, "tools/toolsorigin");

	if (dropped > 0)
	{
		rMessage() << fmt::format("[vmf]: Dropped {0} hint, skip and origin brushes from the world", dropped) << std::endl;
	}

	auto worldBounds = getBounds(data.world.solids);

	auto scale = ConversionMap::getScale();

	convertTargets(data.entities);

	auto world = createEntity(data.world, "vmf");

	for (const auto& solid : data.world.solids)
	{
		addSolid(solid, Vector3(0, 0, 0), world, solid.visgroups, solid.groups, assigner);
	}

	_importFilter.addEntity(world);

	for (auto& entity : data.entities)
	{
		auto origin = takeOrigin(entity, "tools/toolsorigin");

		convertSolidity(entity);
		convertSourceLight(entity, worldBounds, scale);
		convertMoveDirection(entity);
		convertRendering(entity);

		auto node = createEntity(entity, "vmf");

		for (const auto& solid : entity.solids)
		{
			addSolid(solid, origin, node, entity.visgroups, {}, assigner);
		}

		_importFilter.addEntity(node);
		assigner.assign(node, entity.visgroups, entity.groups);
	}
}

void ValveVmfReader::addSolid(const Solid& solid, const Vector3& origin, const scene::INodePtr& entity,
	const std::vector<int>& visgroups, const std::vector<int>& groups, SceneAssigner& assigner)
{
	bool displaced = std::any_of(solid.faces.begin(), solid.faces.end(), [](const Face& face)
	{
		return face.displacement.has_value();
	});

	if (!displaced)
	{
		auto brush = createBrush(solid, origin, {});
		_importFilter.addPrimitiveToEntity(brush, entity);
		assigner.assign(brush, visgroups, groups);
		return;
	}

	for (const auto& face : solid.faces)
	{
		if (face.displacement)
		{
			auto patch = createDisplacement(face, origin);
			_importFilter.addPrimitiveToEntity(patch, entity);
			assigner.assign(patch, visgroups, groups);
		}
	}
}

// Format

const std::string& ValveVmfFormat::getMapFormatName() const
{
	static std::string _name = "Valve VMF";
	return _name;
}

const std::string& ValveVmfFormat::getGameType() const
{
	static std::string _gameType = "valvevmf";
	return _gameType;
}

std::string ValveVmfFormat::getName() const
{
	return "ValveVmfLoader";
}

StringSet ValveVmfFormat::getDependencies() const
{
	static StringSet _dependencies;
	if (_dependencies.empty())
	{
		_dependencies.insert(MODULE_MAPFORMATMANAGER);
	}
	return _dependencies;
}

void ValveVmfFormat::initialiseModule(const IApplicationContext& ctx)
{
	GlobalMapFormatManager().registerMapFormat("vmf", shared_from_this());
}

void ValveVmfFormat::shutdownModule()
{
	GlobalMapFormatManager().unregisterMapFormat(shared_from_this());
}

IMapReaderPtr ValveVmfFormat::getMapReader(IMapImportFilter& filter) const
{
	return std::make_shared<ValveVmfReader>(filter);
}

IMapWriterPtr ValveVmfFormat::getMapWriter() const
{
	return std::make_shared<Quake3MapWriter>();
}

bool ValveVmfFormat::allowInfoFileCreation() const
{
	return false;
}

bool ValveVmfFormat::canLoad(std::istream& stream) const
{
	return false;
}

module::StaticModuleRegistration<ValveVmfFormat> vmfMapModule;

} // namespace map
