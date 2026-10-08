#include "GoldSrcFormat.h"

#include "Quake3MapWriter.h"
#include "ConversionMap.h"
#include "goldsrc/GoldSrcConversion.h"
#include "module/StaticModule.h"
#include "imap.h"
#include "itextstream.h"

#include <fmt/format.h>

#include <iterator>
#include <map>
#include <set>

namespace map
{

using namespace goldsrc;

namespace
{

const std::set<std::string> NON_SOLID_CLASSES = {
	"func_illusionary", "info_location", "info_join_team", "info_join_autoassign", "info_spectate",
	"func_buyzone", "func_bomb_target", "func_hostage_rescue", "func_vip_safetyzone", "func_escapezone",
	"func_mortar_field", "env_bubbles"
};

const std::map<std::string, std::string> CLASS_MATERIALS = {
	{ "func_ladder", "textures/common/ladder" },
	{ "func_monsterclip", "textures/common/monster_clip" },
};

void convertSolidity(Entity& entity)
{
	auto noclip = parseNumbers(findValue(entity, "zhlt_noclip"));
	bool nonSolid = NON_SOLID_CLASSES.count(entity.keyValues.front().second) > 0 || (!noclip.empty() && noclip[0] != 0);

	removeKey(entity, "zhlt_noclip");

	if (nonSolid && !entity.solids.empty())
	{
		setValue(entity, "solid", "0");
	}
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

	SceneAssigner assigner(_importFilter.getRootNode(), data.visgroups);

	auto dropped = removeToolSolids(data.world.solids, { "hint", "skip" }, "origin");

	if (dropped > 0)
	{
		rMessage() << fmt::format("[goldsrc]: Dropped {0} hint, skip and origin brushes from the world", dropped) << std::endl;
	}

	auto worldBounds = getBounds(data.world.solids);

	auto scale = ConversionMap::getScale();

	convertTargets(data.entities);

	auto world = createEntity(data.world, "goldsrc");

	for (const auto& solid : data.world.solids)
	{
		auto brush = createBrush(solid, Vector3(0, 0, 0), {});
		_importFilter.addPrimitiveToEntity(brush, world);
		assigner.assign(brush, solid.visgroups, solid.groups);
	}

	_importFilter.addEntity(world);

	for (auto& entity : data.entities)
	{
		auto origin = takeOrigin(entity, "origin");

		auto forcedMaterial = CLASS_MATERIALS.find(entity.keyValues.front().second);
		auto material = forcedMaterial != CLASS_MATERIALS.end() ? forcedMaterial->second : std::string();

		convertSolidity(entity);
		convertLight(entity, worldBounds, scale, "_cone2", 0);
		convertAngles(entity, !entity.solids.empty());
		convertRendering(entity);

		auto node = createEntity(entity, "goldsrc");

		for (const auto& solid : entity.solids)
		{
			auto brush = createBrush(solid, origin, material);
			_importFilter.addPrimitiveToEntity(brush, node);
			assigner.assign(brush, entity.visgroups, {});
		}

		_importFilter.addEntity(node);
		assigner.assign(node, entity.visgroups, entity.groups);
	}
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
