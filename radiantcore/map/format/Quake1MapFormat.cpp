#include "Quake1MapFormat.h"

#include "Quake3MapWriter.h"
#include "primitiveparsers/Quake1BrushDef.h"
#include "module/StaticModule.h"

namespace map
{

// Readers

void Quake1MapReader::initPrimitiveParsers()
{
	if (_primitiveParsers.empty())
	{
		addPrimitiveParser(std::make_shared<Quake1BrushDefParser>());
	}
}

void Valve220MapReader::initPrimitiveParsers()
{
	if (_primitiveParsers.empty())
	{
		addPrimitiveParser(std::make_shared<Valve220BrushDefParser>());
	}
}

void Quake2MapReader::initPrimitiveParsers()
{
	if (_primitiveParsers.empty())
	{
		addPrimitiveParser(std::make_shared<Quake2BrushDefParser>());
	}
}

// Quake 1

const std::string& Quake1MapFormat::getMapFormatName() const
{
	static std::string _name = "Quake 1";
	return _name;
}

const std::string& Quake1MapFormat::getGameType() const
{
	static std::string _gameType = "quake1";
	return _gameType;
}

std::string Quake1MapFormat::getName() const
{
	return "Quake1MapLoader";
}

StringSet Quake1MapFormat::getDependencies() const
{
	static StringSet _dependencies;
	if (_dependencies.empty())
	{
		_dependencies.insert(MODULE_MAPFORMATMANAGER);
	}
	return _dependencies;
}

void Quake1MapFormat::initialiseModule(const IApplicationContext& ctx)
{
	GlobalMapFormatManager().registerMapFormat("map", shared_from_this());
}

void Quake1MapFormat::shutdownModule()
{
	GlobalMapFormatManager().unregisterMapFormat(shared_from_this());
}

IMapReaderPtr Quake1MapFormat::getMapReader(IMapImportFilter& filter) const
{
	return std::make_shared<Quake1MapReader>(filter);
}

IMapWriterPtr Quake1MapFormat::getMapWriter() const
{
	return std::make_shared<Quake3MapWriter>();
}

bool Quake1MapFormat::allowInfoFileCreation() const
{
	return false;
}

bool Quake1MapFormat::canLoad(std::istream& stream) const
{
	return false;
}

// Valve 220

const std::string& Valve220MapFormat::getMapFormatName() const
{
	static std::string _name = "Valve 220";
	return _name;
}

const std::string& Valve220MapFormat::getGameType() const
{
	static std::string _gameType = "valve220";
	return _gameType;
}

std::string Valve220MapFormat::getName() const
{
	return "Valve220MapLoader";
}

StringSet Valve220MapFormat::getDependencies() const
{
	static StringSet _dependencies;
	if (_dependencies.empty())
	{
		_dependencies.insert(MODULE_MAPFORMATMANAGER);
	}
	return _dependencies;
}

void Valve220MapFormat::initialiseModule(const IApplicationContext& ctx)
{
	GlobalMapFormatManager().registerMapFormat("map", shared_from_this());
}

void Valve220MapFormat::shutdownModule()
{
	GlobalMapFormatManager().unregisterMapFormat(shared_from_this());
}

IMapReaderPtr Valve220MapFormat::getMapReader(IMapImportFilter& filter) const
{
	return std::make_shared<Valve220MapReader>(filter);
}

IMapWriterPtr Valve220MapFormat::getMapWriter() const
{
	return std::make_shared<Quake3MapWriter>();
}

bool Valve220MapFormat::allowInfoFileCreation() const
{
	return false;
}

bool Valve220MapFormat::canLoad(std::istream& stream) const
{
	return false;
}

// Quake 2

const std::string& Quake2MapFormat::getMapFormatName() const
{
	static std::string _name = "Quake 2";
	return _name;
}

const std::string& Quake2MapFormat::getGameType() const
{
	static std::string _gameType = "quake2";
	return _gameType;
}

std::string Quake2MapFormat::getName() const
{
	return "Quake2MapLoader";
}

StringSet Quake2MapFormat::getDependencies() const
{
	static StringSet _dependencies;
	if (_dependencies.empty())
	{
		_dependencies.insert(MODULE_MAPFORMATMANAGER);
	}
	return _dependencies;
}

void Quake2MapFormat::initialiseModule(const IApplicationContext& ctx)
{
	GlobalMapFormatManager().registerMapFormat("map", shared_from_this());
}

void Quake2MapFormat::shutdownModule()
{
	GlobalMapFormatManager().unregisterMapFormat(shared_from_this());
}

IMapReaderPtr Quake2MapFormat::getMapReader(IMapImportFilter& filter) const
{
	return std::make_shared<Quake2MapReader>(filter);
}

IMapWriterPtr Quake2MapFormat::getMapWriter() const
{
	return std::make_shared<Quake3MapWriter>();
}

bool Quake2MapFormat::allowInfoFileCreation() const
{
	return false;
}

bool Quake2MapFormat::canLoad(std::istream& stream) const
{
	return false;
}

module::StaticModuleRegistration<Quake1MapFormat> q1MapModule;
module::StaticModuleRegistration<Valve220MapFormat> valve220MapModule;
module::StaticModuleRegistration<Quake2MapFormat> q2MapModule;

} // namespace map
