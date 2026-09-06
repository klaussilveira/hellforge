#pragma once

#include "imapformat.h"

#include <map>
#include <set>
#include <string>
#include <vector>

namespace map
{

namespace doom
{

std::vector<std::string> getWadMapNames(const std::string& path);

void scanWadMap(const std::string& path, const std::string& mapName,
	std::set<std::string>& textures, std::set<std::string>& entities);

}

class DoomWadReader :
	public IMapReader
{
	IMapImportFilter& _importFilter;

public:
	DoomWadReader(IMapImportFilter& importFilter);

	void readFromStream(std::istream& stream) override;

private:
	typedef std::map<std::string, std::string> EntityKeyValues;

	scene::INodePtr createEntity(const EntityKeyValues& keyValues);
};

class DoomWadFormat :
	public MapFormat,
	public std::enable_shared_from_this<DoomWadFormat>
{
public:
	std::string getName() const override;
	StringSet getDependencies() const override;
	void initialiseModule(const IApplicationContext& ctx) override;
	void shutdownModule() override;

	const std::string& getMapFormatName() const override;
	const std::string& getGameType() const override;
	IMapReaderPtr getMapReader(IMapImportFilter& filter) const override;
	IMapWriterPtr getMapWriter() const override;
	bool allowInfoFileCreation() const override;
	bool canLoad(std::istream& stream) const override;
};

}
