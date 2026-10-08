#pragma once

#include "imapformat.h"
#include "goldsrc/GoldSrcLoader.h"

namespace map
{

class GoldSrcReader :
	public IMapReader
{
	IMapImportFilter& _importFilter;

public:
	GoldSrcReader(IMapImportFilter& importFilter);

	void readFromStream(std::istream& stream) override;

private:
	scene::INodePtr createEntity(const goldsrc::Entity& entity);
	scene::INodePtr createBrush(const goldsrc::Solid& solid, const Vector3& origin, const std::string& material);
};

class GoldSrcFormat :
	public MapFormat,
	public std::enable_shared_from_this<GoldSrcFormat>
{
	std::string _formatName;
	std::string _extension;
	std::string _moduleName;

public:
	GoldSrcFormat(const std::string& formatName, const std::string& extension, const std::string& moduleName);

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

class WorldcraftRmfFormat :
	public GoldSrcFormat
{
public:
	WorldcraftRmfFormat();
};

class JackhammerJmfFormat :
	public GoldSrcFormat
{
public:
	JackhammerJmfFormat();
};

}
