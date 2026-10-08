#include "GoldSrcFormat.h"

#include "Quake3MapWriter.h"
#include "Quake3Utils.h"
#include "ConversionMap.h"
#include "primitiveparsers/Quake1BrushDef.h"
#include "module/StaticModule.h"
#include "imap.h"
#include "ibrush.h"
#include "ieclass.h"
#include "ilayer.h"
#include "iselectiongroup.h"
#include "itextstream.h"
#include "scene/EntityNode.h"
#include "string/case_conv.h"

#include <fmt/format.h>

#include <algorithm>
#include <cmath>
#include <iterator>
#include <map>
#include <set>
#include <sstream>

namespace map
{

namespace
{

const double DEFAULT_LIGHT_BRIGHTNESS = 200;
const double LIGHT_RADIUS_PER_BRIGHTNESS = 2;
const double DEFAULT_SPOT_CONE = 45;
const double MAX_SPOT_CONE = 85;

const std::set<std::string> NON_SOLID_CLASSES = {
	"func_illusionary", "info_location", "info_join_team", "info_join_autoassign", "info_spectate",
	"func_buyzone", "func_bomb_target", "func_hostage_rescue", "func_vip_safetyzone", "func_escapezone",
	"func_mortar_field", "env_bubbles"
};

const std::map<std::string, std::string> CLASS_MATERIALS = {
	{ "func_ladder", "textures/common/ladder" },
	{ "func_monsterclip", "textures/common/monster_clip" },
};

std::string* findValue(goldsrc::Entity& entity, const std::string& key)
{
	for (auto& pair : entity.keyValues)
	{
		if (pair.first == key) return &pair.second;
	}

	return nullptr;
}

void setValue(goldsrc::Entity& entity, const std::string& key, const std::string& value)
{
	if (auto existing = findValue(entity, key))
	{
		*existing = value;
		return;
	}

	entity.keyValues.emplace_back(key, value);
}

void removeKey(goldsrc::Entity& entity, const std::string& key)
{
	auto& pairs = entity.keyValues;
	pairs.erase(std::remove_if(pairs.begin(), pairs.end(),
		[&](const auto& pair) { return pair.first == key; }), pairs.end());
}

std::vector<double> parseNumbers(const std::string* value)
{
	std::vector<double> numbers;

	if (value)
	{
		std::stringstream stream(*value);
		double number;

		while (stream >> number)
		{
			numbers.push_back(number);
		}
	}

	return numbers;
}

std::string formatVector(const Vector3& v)
{
	return fmt::format("{0:g} {1:g} {2:g}", v.x(), v.y(), v.z());
}

double toRadians(double degrees)
{
	return degrees * math::PI / 180;
}

Vector3 lightDirection(goldsrc::Entity& entity)
{
	auto angles = parseNumbers(findValue(entity, "angles"));
	auto angle = parseNumbers(findValue(entity, "angle"));
	auto pitchKey = parseNumbers(findValue(entity, "pitch"));

	double yaw = !angle.empty() && angle[0] != 0 ? angle[0] : (angles.size() == 3 ? angles[1] : 0);
	double pitch = !pitchKey.empty() && pitchKey[0] != 0 ? pitchKey[0] : (angles.size() == 3 ? angles[0] : 0);

	if (yaw == -1) return Vector3(0, 0, 1);
	if (yaw == -2) return Vector3(0, 0, -1);

	return Vector3(std::cos(toRadians(yaw)) * std::cos(toRadians(pitch)),
		std::sin(toRadians(yaw)) * std::cos(toRadians(pitch)),
		std::sin(toRadians(pitch)));
}

void convertLight(goldsrc::Entity& entity, const AABB& worldBounds, double scale)
{
	auto light = parseNumbers(findValue(entity, "_light"));

	if (light.empty())
	{
		return;
	}

	Vector3 color(1, 1, 1);
	double brightness = DEFAULT_LIGHT_BRIGHTNESS;

	if (light.size() >= 3)
	{
		color = Vector3(light[0], light[1], light[2]) / 255;
	}

	if (light.size() >= 4)
	{
		brightness = light[3];
	}
	else if (light.size() == 1)
	{
		brightness = light[0];
	}

	setValue(entity, "_color", formatVector(color));

	auto radius = brightness * LIGHT_RADIUS_PER_BRIGHTNESS;
	auto className = entity.keyValues.front().second;

	if (className == "light_environment" && worldBounds.isValid())
	{
		auto extents = worldBounds.getExtents();
		auto reach = std::max({ extents.x(), extents.y(), extents.z() });

		setValue(entity, "parallel", "1");
		setValue(entity, "origin", formatVector(worldBounds.getOrigin()));
		setValue(entity, "light_radius", formatVector(extents));
		setValue(entity, "light_center", formatVector(-lightDirection(entity) * reach));
	}
	else if (className == "light_spot")
	{
		auto cone = parseNumbers(findValue(entity, "_cone2"));
		auto halfAngle = std::min(cone.empty() || cone[0] <= 0 ? DEFAULT_SPOT_CONE : cone[0], MAX_SPOT_CONE);

		auto direction = lightDirection(entity);
		auto right = direction.cross(std::abs(direction.z()) > 0.999 ? Vector3(1, 0, 0) : Vector3(0, 0, 1)).getNormalised();
		auto up = right.cross(direction);

		auto length = radius * scale;
		auto spread = length * std::tan(toRadians(halfAngle));

		setValue(entity, "light_target", formatVector(direction * length));
		setValue(entity, "light_right", formatVector(right * spread));
		setValue(entity, "light_up", formatVector(up * spread));
	}
	else
	{
		setValue(entity, "light_radius", formatVector(Vector3(radius, radius, radius)));
	}

	for (const char* key : { "_light", "_cone", "_cone2", "pitch", "angles", "angle" })
	{
		removeKey(entity, key);
	}
}

void convertAngles(goldsrc::Entity& entity, bool isBrushEntity)
{
	auto angles = parseNumbers(findValue(entity, "angles"));
	auto angle = parseNumbers(findValue(entity, "angle"));

	if (isBrushEntity)
	{
		if (angles.size() != 3 && angle.empty())
		{
			return;
		}

		double movedir = angle.empty() ? angles[1] : angle[0];

		if (angles.size() == 3 && angle.empty())
		{
			if (angles[0] == -90) movedir = -1;
			if (angles[0] == 90) movedir = -2;
		}

		setValue(entity, "movedir", fmt::format("{0:g}", movedir));
		removeKey(entity, "angles");
		removeKey(entity, "angle");
		return;
	}

	if (angles.size() != 3)
	{
		return;
	}

	removeKey(entity, "angles");

	if (angles[0] == 0 && angles[2] == 0)
	{
		setValue(entity, "angle", fmt::format("{0:g}", angles[1]));
		return;
	}

	double sp = std::sin(toRadians(angles[0])), cp = std::cos(toRadians(angles[0]));
	double sy = std::sin(toRadians(angles[1])), cy = std::cos(toRadians(angles[1]));
	double sr = std::sin(toRadians(angles[2])), cr = std::cos(toRadians(angles[2]));

	setValue(entity, "rotation", fmt::format("{0:g} {1:g} {2:g} {3:g} {4:g} {5:g} {6:g} {7:g} {8:g}",
		cp * cy, cp * sy, -sp,
		sr * sp * cy - cr * sy, sr * sp * sy + cr * cy, sr * cp,
		cr * sp * cy + sr * sy, cr * sp * sy - sr * cy, cr * cp));
}

void convertRendering(goldsrc::Entity& entity)
{
	auto mode = parseNumbers(findValue(entity, "rendermode"));
	auto amount = parseNumbers(findValue(entity, "renderamt"));
	auto color = parseNumbers(findValue(entity, "rendercolor"));

	if (!mode.empty() && mode[0] != 0 && mode[0] != 4)
	{
		setValue(entity, "shaderParm3", fmt::format("{0:g}", (amount.empty() ? 0 : amount[0]) / 255));

		if (color.size() == 3 && (color[0] != 0 || color[1] != 0 || color[2] != 0) && !findValue(entity, "_color"))
		{
			setValue(entity, "_color", formatVector(Vector3(color[0], color[1], color[2]) / 255));
		}
	}

	removeKey(entity, "rendermode");
	removeKey(entity, "renderamt");
	removeKey(entity, "rendercolor");
}

void convertTargets(std::vector<goldsrc::Entity>& entities)
{
	std::map<std::string, std::vector<std::size_t>> named;

	for (std::size_t i = 0; i < entities.size(); ++i)
	{
		auto name = findValue(entities[i], "targetname");

		if (name && !name->empty())
		{
			named[*name].push_back(i);
		}
	}

	for (const auto& [name, owners] : named)
	{
		for (std::size_t k = 0; k < owners.size(); ++k)
		{
			auto& entity = entities[owners[k]];
			setValue(entity, "name", owners.size() > 1 ? fmt::format("{0}_{1}", name, k + 1) : name);
			removeKey(entity, "targetname");
		}
	}

	for (auto& entity : entities)
	{
		auto target = findValue(entity, "target");

		if (!target)
		{
			continue;
		}

		auto found = named.find(*target);

		if (found == named.end() || found->second.size() < 2)
		{
			continue;
		}

		auto name = *target;

		for (std::size_t k = 0; k < found->second.size(); ++k)
		{
			setValue(entity, k == 0 ? "target" : fmt::format("target{0}", k), fmt::format("{0}_{1}", name, k + 1));
		}
	}
}

void convertSolidity(goldsrc::Entity& entity)
{
	auto noclip = parseNumbers(findValue(entity, "zhlt_noclip"));
	bool nonSolid = NON_SOLID_CLASSES.count(entity.keyValues.front().second) > 0 || (!noclip.empty() && noclip[0] != 0);

	removeKey(entity, "zhlt_noclip");

	if (nonSolid && !entity.solids.empty())
	{
		setValue(entity, "solid", "0");
	}
}

bool usesOnly(const goldsrc::Solid& solid, const std::set<std::string>& textures)
{
	if (solid.faces.empty())
	{
		return false;
	}

	for (const auto& face : solid.faces)
	{
		if (textures.count(string::to_lower_copy(face.texture)) == 0)
		{
			return false;
		}
	}

	return true;
}

Vector3 takeOrigin(goldsrc::Entity& entity)
{
	auto& solids = entity.solids;

	for (auto solid = solids.begin(); solid != solids.end(); ++solid)
	{
		if (solid->bounds.isValid() && usesOnly(*solid, { "origin" }))
		{
			auto origin = solid->bounds.getOrigin();
			solids.erase(solid);
			setValue(entity, "origin", formatVector(origin));
			return origin;
		}
	}

	auto origin = parseNumbers(findValue(entity, "origin"));

	return origin.size() == 3 && !solids.empty() ? Vector3(origin[0], origin[1], origin[2]) : Vector3(0, 0, 0);
}

}

GoldSrcReader::GoldSrcReader(IMapImportFilter& importFilter) :
	_importFilter(importFilter)
{}

void GoldSrcReader::readFromStream(std::istream& stream)
{
	std::string contents{ std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>() };

	auto data = goldsrc::load(contents);

	if (data.skippedPaths > 0)
	{
		rWarning() << fmt::format("[goldsrc]: Skipped {0} paths, path conversion is not supported", data.skippedPaths) << std::endl;
	}

	if (data.skippedCurves > 0)
	{
		rWarning() << fmt::format("[goldsrc]: Skipped {0} curves, curve conversion is not supported", data.skippedCurves) << std::endl;
	}

	const auto& root = _importFilter.getRootNode();
	auto& layerManager = root->getLayerManager();
	auto& groupManager = root->getSelectionGroupManager();

	std::map<int, int> layerIds;

	for (const auto& visgroup : data.visgroups)
	{
		auto layerId = layerManager.getLayerID(visgroup.name);

		if (layerId == -1)
		{
			layerId = layerManager.createLayer(visgroup.name);
		}

		if (layerId != -1)
		{
			layerIds[visgroup.id] = layerId;

			if (!visgroup.visible)
			{
				layerManager.setLayerVisibility(layerId, false);
			}
		}
	}

	std::map<int, selection::ISelectionGroupPtr> groups;

	auto assign = [&](const scene::INodePtr& node, const std::vector<int>& visgroups, const std::vector<int>& groupChain)
	{
		scene::LayerList layers;

		for (auto visgroup : visgroups)
		{
			auto found = layerIds.find(visgroup);

			if (found != layerIds.end())
			{
				layers.insert(found->second);
			}
		}

		if (!layers.empty())
		{
			node->assignToLayers(layers);
		}

		for (auto id : groupChain)
		{
			auto& group = groups[id];

			if (!group)
			{
				group = groupManager.createSelectionGroup();
			}

			group->addNode(node);
		}
	};

	auto& worldSolids = data.world.solids;
	auto worldSolidCount = worldSolids.size();

	worldSolids.erase(std::remove_if(worldSolids.begin(), worldSolids.end(), [](const goldsrc::Solid& solid)
	{
		return usesOnly(solid, { "hint", "skip" }) || usesOnly(solid, { "origin" });
	}), worldSolids.end());

	if (worldSolids.size() < worldSolidCount)
	{
		rMessage() << fmt::format("[goldsrc]: Dropped {0} hint, skip and origin brushes from the world", worldSolidCount - worldSolids.size()) << std::endl;
	}

	AABB worldBounds;

	for (const auto& solid : data.world.solids)
	{
		worldBounds.includeAABB(solid.bounds);
	}

	auto scale = ConversionMap::getScale();

	convertTargets(data.entities);

	auto world = createEntity(data.world);

	for (const auto& solid : data.world.solids)
	{
		auto brush = createBrush(solid, Vector3(0, 0, 0), {});
		_importFilter.addPrimitiveToEntity(brush, world);
		assign(brush, solid.visgroups, solid.groups);
	}

	_importFilter.addEntity(world);

	for (auto& entity : data.entities)
	{
		auto origin = takeOrigin(entity);

		auto forcedMaterial = CLASS_MATERIALS.find(entity.keyValues.front().second);
		auto material = forcedMaterial != CLASS_MATERIALS.end() ? forcedMaterial->second : std::string();

		convertSolidity(entity);
		convertLight(entity, worldBounds, scale);
		convertAngles(entity, !entity.solids.empty());
		convertRendering(entity);

		auto node = createEntity(entity);

		for (const auto& solid : entity.solids)
		{
			auto brush = createBrush(solid, origin, material);
			_importFilter.addPrimitiveToEntity(brush, node);
			assign(brush, entity.visgroups, {});
		}

		_importFilter.addEntity(node);
		assign(node, entity.visgroups, entity.groups);
	}
}

scene::INodePtr GoldSrcReader::createEntity(const goldsrc::Entity& entity)
{
	const auto& className = entity.keyValues.front().second;
	auto classPtr = GlobalEntityClassManager().findClass(className);

	if (!classPtr)
	{
		rError() << "[goldsrc]: Could not find entity class: " << className << std::endl;

		classPtr = GlobalEntityClassManager().findOrInsert(className, true);
	}

	EntityNodePtr node(GlobalEntityModule().createEntity(classPtr));

	for (const auto& pair : entity.keyValues)
	{
		node->getEntity().setKeyValue(pair.first, ConversionMap::scaleSpatialValue(pair.first, pair.second));
	}

	return node;
}

scene::INodePtr GoldSrcReader::createBrush(const goldsrc::Solid& solid, const Vector3& origin, const std::string& material)
{
	auto node = GlobalBrushCreator().createBrush();
	auto& brush = *Node_getIBrush(node);

	auto scale = ConversionMap::getScale();

	for (const auto& face : solid.faces)
	{
		const auto& normal = face.plane.normal();
		Plane3 plane(normal, (face.plane.dist() - normal.dot(origin)) * scale);

		auto shader = material.empty() ? resolveTextureName(face.texture) : material;

		auto uAxis = face.uAxis;
		auto vAxis = face.vAxis;

		if (!face.valveAxes)
		{
			ShiftScaleRotation rotation;
			rotation.shift[0] = 0;
			rotation.shift[1] = 0;
			rotation.rotate = face.rotation;
			rotation.scale[0] = 1;
			rotation.scale[1] = 1;

			double axes[2][4];
			quake3::getTextureVectorsForFace(normal, rotation, 1, 1, axes);

			uAxis = Vector3(axes[0][0], axes[0][1], axes[0][2]);
			vAxis = Vector3(axes[1][0], axes[1][1], axes[1][2]);
		}

		auto scaleU = face.scaleU == 0 ? 1 : face.scaleU;
		auto scaleV = face.scaleV == 0 ? 1 : face.scaleV;

		auto sourceSize = ConversionMap::findTextureSize(string::to_upper_copy(face.texture));
		auto [imageWidth, imageHeight] = sourceSize ? *sourceSize : std::pair<double, double>(getTextureDimensions(shader));

		auto texdef = Valve220BrushDefParser::calculateTextureMatrix(imageWidth, imageHeight, plane.normal(),
			uAxis, vAxis, face.shiftU, face.shiftV, scaleU * scale, scaleV * scale);

		brush.addFace(plane, texdef, shader);
	}

	brush.removeRedundantFaces();
	return node;
}

GoldSrcFormat::GoldSrcFormat(const std::string& formatName, const std::string& extension, const std::string& moduleName) :
	_formatName(formatName),
	_extension(extension),
	_moduleName(moduleName)
{}

const std::string& GoldSrcFormat::getMapFormatName() const
{
	return _formatName;
}

const std::string& GoldSrcFormat::getGameType() const
{
	static std::string _gameType = "goldsrc";
	return _gameType;
}

std::string GoldSrcFormat::getName() const
{
	return _moduleName;
}

StringSet GoldSrcFormat::getDependencies() const
{
	return { MODULE_MAPFORMATMANAGER };
}

void GoldSrcFormat::initialiseModule(const IApplicationContext& ctx)
{
	GlobalMapFormatManager().registerMapFormat(_extension, shared_from_this());
}

void GoldSrcFormat::shutdownModule()
{
	GlobalMapFormatManager().unregisterMapFormat(shared_from_this());
}

IMapReaderPtr GoldSrcFormat::getMapReader(IMapImportFilter& filter) const
{
	return std::make_shared<GoldSrcReader>(filter);
}

IMapWriterPtr GoldSrcFormat::getMapWriter() const
{
	return std::make_shared<Quake3MapWriter>();
}

bool GoldSrcFormat::allowInfoFileCreation() const
{
	return false;
}

bool GoldSrcFormat::canLoad(std::istream& stream) const
{
	return false;
}

WorldcraftRmfFormat::WorldcraftRmfFormat() :
	GoldSrcFormat("Worldcraft RMF", "rmf", "WorldcraftRmfLoader")
{}

JackhammerJmfFormat::JackhammerJmfFormat() :
	GoldSrcFormat("Jackhammer JMF", "jmf", "JackhammerJmfLoader")
{}

module::StaticModuleRegistration<WorldcraftRmfFormat> worldcraftRmfModule;
module::StaticModuleRegistration<JackhammerJmfFormat> jackhammerJmfModule;

}
