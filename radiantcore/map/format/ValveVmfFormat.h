#pragma once

#include "imapformat.h"

namespace map
{

class ValveVmfReader :
	public IMapReader
{
	IMapImportFilter& _importFilter;
	std::size_t _entityCount;

public:
	ValveVmfReader(IMapImportFilter& importFilter);
	void readFromStream(std::istream& stream) override;

private:
	typedef std::map<std::string, std::string> EntityKeyValues;

	void skipBlock(parser::DefTokeniser& tok);
	void parseWorldOrEntity(parser::DefTokeniser& tok, bool isWorld);
	scene::INodePtr parseSolid(parser::DefTokeniser& tok);
	scene::INodePtr createEntity(const EntityKeyValues& keyValues);
};

class ValveVmfFormat :
	public MapFormat,
	public std::enable_shared_from_this<ValveVmfFormat>
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

} // namespace map
