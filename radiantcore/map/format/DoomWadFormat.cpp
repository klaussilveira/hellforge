#include "DoomWadFormat.h"

#include "Quake3MapWriter.h"
#include "WadImportSettings.h"
#include "doom/DoomBinaryLoader.h"
#include "doom/DoomBrushBuilder.h"
#include "doom/DoomLightPlacer.h"
#include "doom/DoomThingTable.h"
#include "doom/SectorGeometry.h"
#include "doom/UdmfLoader.h"
#include "doom/WadFile.h"

#include "module/StaticModule.h"
#include "messages/MapFileOperation.h"
#include "ieclass.h"
#include "iradiant.h"
#include "itextstream.h"
#include "i18n.h"
#include "scene/EntityNode.h"

#include <fmt/format.h>

#include <algorithm>
#include <fstream>

namespace map
{

using namespace doom;

namespace
{

const double PRISM_MARGIN = 8;

const double SHELL_THICKNESS = 16;

void reportProgress(const std::string& text, float fraction)
{
	FileOperation message(FileOperation::Type::Import, FileOperation::Progress, true, fraction);
	message.setText(text);

	GlobalRadiantCore().getMessageBus().sendMessage(message);
}

int findSectorAt(const std::vector<SectorFootprint>& footprints, const Vector2& point)
{
	for (std::size_t i = 0; i < footprints.size(); ++i)
	{
		const auto& footprint = footprints[i];

		if (!footprint.valid ||
			point.x() < footprint.boundsMin.x() || point.x() > footprint.boundsMax.x() ||
			point.y() < footprint.boundsMin.y() || point.y() > footprint.boundsMax.y())
		{
			continue;
		}

		for (const auto& piece : footprint.pieces)
		{
			double sign = polygon::ringArea(piece) >= 0 ? 1.0 : -1.0;
			bool inside = true;

			for (std::size_t edge = 0; edge < piece.size(); ++edge)
			{
				const auto& from = piece[edge];
				const auto& to = piece[(edge + 1) % piece.size()];

				if (sign * (to - from).crossProduct(point - from) < 0)
				{
					inside = false;
					break;
				}
			}

			if (inside)
			{
				return static_cast<int>(i);
			}
		}
	}

	return NO_INDEX;
}

std::string formatVector(double x, double y, double z)
{
	return fmt::format("{0:g} {1:g} {2:g}", x, y, z);
}

}

namespace doom
{

std::vector<std::string> getWadMapNames(const std::string& path)
{
	std::vector<std::string> names;

	std::ifstream stream(path, std::ios::in | std::ios::binary);

	if (!stream)
	{
		return names;
	}

	try
	{
		WadFile wad(stream);

		for (const auto& group : wad.getMaps())
		{
			names.push_back(group.name);
		}
	}
	catch (const IMapReader::FailureException& ex)
	{
		rWarning() << "[wad]: " << ex.what() << std::endl;
	}

	return names;
}

void scanWadMap(const std::string& path, const std::string& mapName,
	std::set<std::string>& textures, std::set<std::string>& entities)
{
	std::ifstream stream(path, std::ios::in | std::ios::binary);

	if (!stream)
	{
		return;
	}

	try
	{
		WadFile wad(stream);

		if (wad.getMaps().empty())
		{
			return;
		}

		const WadMapGroup* group = mapName.empty() ?
			&wad.getMaps().front() : wad.findMap(mapName);

		if (!group)
		{
			return;
		}

		auto data = group->kind == MapLumpKind::Udmf ?
			UdmfLoader::load(wad.readMapLump(*group, "TEXTMAP")) :
			DoomBinaryLoader::load(wad, *group);

		auto collect = [&textures](const std::string& name)
		{
			if (textureIsSet(name))
			{
				textures.insert(name);
			}
		};

		for (const auto& side : data.sideDefs)
		{
			collect(side.upperTexture);
			collect(side.lowerTexture);
			collect(side.middleTexture);
		}

		for (const auto& sector : data.sectors)
		{
			collect(sector.floorTexture);
			collect(sector.ceilingTexture);
		}

		for (const auto& thing : data.things)
		{
			entities.insert(getThingClassName(thing.type));
		}
	}
	catch (const IMapReader::FailureException& ex)
	{
		rWarning() << "[wad]: " << ex.what() << std::endl;
	}
}

}

DoomWadReader::DoomWadReader(IMapImportFilter& importFilter) :
	_importFilter(importFilter)
{}

void DoomWadReader::readFromStream(std::istream& stream)
{
	if (WadImportSettings::wasCancelled())
	{
		throw FileOperation::OperationCancelled();
	}

	WadFile wad(stream);

	if (wad.getMaps().empty())
	{
		throw FailureException(_("This WAD does not contain any maps"));
	}

	const auto& requested = WadImportSettings::getMapName();

	const WadMapGroup* group = requested.empty() ? &wad.getMaps().front() : wad.findMap(requested);

	if (!group)
	{
		throw FailureException(fmt::format(_("The WAD does not contain a map named {0}"), requested));
	}

	rMessage() << "[wad]: importing map " << group->name << std::endl;

	reportProgress(_("Reading map lumps"), 0.05f);

	auto data = group->kind == MapLumpKind::Udmf ?
		UdmfLoader::load(wad.readMapLump(*group, "TEXTMAP")) :
		DoomBinaryLoader::load(wad, *group);

	if (data.sectors.empty() || data.lineDefs.empty())
	{
		throw FailureException(fmt::format(_("Map {0} has no geometry"), group->name));
	}

	rMessage() << "[wad]: " << data.vertices.size() << " vertices, " << data.sectors.size()
		<< " sectors, " << data.lineDefs.size() << " linedefs, " << data.things.size()
		<< " things" << std::endl;

	reportProgress(_("Building sector polygons"), 0.15f);

	auto footprints = SectorGeometry::build(data, PRISM_MARGIN);

	reportProgress(_("Sealing the map boundary"), 0.3f);

	auto shell = SectorGeometry::buildShell(footprints, SHELL_THICKNESS);

	auto scale = WadImportSettings::getScale();

	EntityKeyValues worldKeys;
	worldKeys["classname"] = "worldspawn";

	auto worldspawn = createEntity(worldKeys);

	_importFilter.addEntity(worldspawn);

	reportProgress(_("Creating brushes"), 0.4f);

	std::size_t brushCount = 0;
	std::size_t rejectedCount = 0;

	auto addBrush = [&](const scene::INodePtr& brush)
	{
		if (_importFilter.addPrimitiveToEntity(brush, worldspawn))
		{
			brushCount++;
		}
		else
		{
			rejectedCount++;
		}
	};

	DoomBrushBuilder builder(data, footprints, scale);

	builder.buildSectorBrushes(addBrush);

	reportProgress(_("Creating brushes"), 0.8f);

	builder.buildShellBrushes(shell, SHELL_THICKNESS, addBrush);

	rMessage() << "[wad]: created " << brushCount << " brushes" << std::endl;

	if (rejectedCount > 0)
	{
		rWarning() << "[wad]: " << rejectedCount
			<< " brushes were rejected by the import filter" << std::endl;
	}

	reportProgress(_("Creating entities"), 0.9f);

	std::size_t thingCount = 0;

	for (const auto& thing : data.things)
	{
		Vector2 position(thing.x, thing.y);

		auto sector = findSectorAt(footprints, position);

		auto height = sector != NO_INDEX ? data.sectors[sector].floorHeight : 0;

		EntityKeyValues keyValues;
		keyValues["classname"] = getThingClassName(thing.type);
		keyValues["origin"] = formatVector(thing.x * scale, thing.y * scale, height * scale);

		if (thing.angle != 0)
		{
			keyValues["angle"] = fmt::format("{0:g}", thing.angle);
		}

		if (auto entity = createEntity(keyValues); entity)
		{
			_importFilter.addEntity(entity);
			thingCount++;
		}
	}

	rMessage() << "[wad]: created " << thingCount << " entities from things" << std::endl;

	auto lightSpacing = WadImportSettings::getLightSpacing();

	if (lightSpacing > 0)
	{
		reportProgress(_("Placing lights"), 0.95f);

		auto lights = DoomLightPlacer::place(data, footprints, lightSpacing);

		for (const auto& light : lights)
		{
			auto radius = light.radius * scale;
			auto intensity = std::min(1.0, std::max(0.0, light.lightLevel / 255.0));

			EntityKeyValues keyValues;
			keyValues["classname"] = "light";
			keyValues["origin"] = formatVector(light.position.x() * scale,
				light.position.y() * scale, light.height * scale);
			keyValues["light_radius"] = formatVector(radius, radius, radius);
			keyValues["_color"] = formatVector(intensity, intensity, intensity);

			if (auto entity = createEntity(keyValues); entity)
			{
				_importFilter.addEntity(entity);
			}
		}

		rMessage() << "[wad]: placed " << lights.size() << " lights" << std::endl;
	}
}

scene::INodePtr DoomWadReader::createEntity(const EntityKeyValues& keyValues)
{
	auto found = keyValues.find("classname");

	if (found == keyValues.end())
	{
		throw FailureException("DoomWadReader::createEntity(): could not find classname.");
	}

	const auto& className = found->second;

	auto classPtr = GlobalEntityClassManager().findClass(className);

	if (!classPtr)
	{
		classPtr = GlobalEntityClassManager().findOrInsert(className, true);
	}

	EntityNodePtr node(GlobalEntityModule().createEntity(classPtr));

	for (const auto& pair : keyValues)
	{
		node->getEntity().setKeyValue(pair.first, pair.second);
	}

	return node;
}

const std::string& DoomWadFormat::getMapFormatName() const
{
	static std::string _name = "Doom WAD";
	return _name;
}

const std::string& DoomWadFormat::getGameType() const
{
	static std::string _gameType = "doomwad";
	return _gameType;
}

std::string DoomWadFormat::getName() const
{
	return "DoomWadLoader";
}

StringSet DoomWadFormat::getDependencies() const
{
	static StringSet _dependencies;

	if (_dependencies.empty())
	{
		_dependencies.insert(MODULE_MAPFORMATMANAGER);
	}

	return _dependencies;
}

void DoomWadFormat::initialiseModule(const IApplicationContext& ctx)
{
	GlobalMapFormatManager().registerMapFormat("wad", shared_from_this());
}

void DoomWadFormat::shutdownModule()
{
	GlobalMapFormatManager().unregisterMapFormat(shared_from_this());
}

IMapReaderPtr DoomWadFormat::getMapReader(IMapImportFilter& filter) const
{
	return std::make_shared<DoomWadReader>(filter);
}

IMapWriterPtr DoomWadFormat::getMapWriter() const
{
	return std::make_shared<Quake3MapWriter>();
}

bool DoomWadFormat::allowInfoFileCreation() const
{
	return false;
}

bool DoomWadFormat::canLoad(std::istream& stream) const
{
	return doom::WadFile::hasWadSignature(stream);
}

module::StaticModuleRegistration<DoomWadFormat> doomWadModule;

}
