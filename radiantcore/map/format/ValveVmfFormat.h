#pragma once

#include "imapformat.h"
#include "goldsrc/GoldSrcConversion.h"

namespace map
{

class ValveVmfReader :
	public IMapReader
{
	IMapImportFilter& _importFilter;

public:
	ValveVmfReader(IMapImportFilter& importFilter);
	void readFromStream(std::istream& stream) override;

private:
	void addSolid(const goldsrc::Solid& solid, const Vector3& origin, const scene::INodePtr& entity,
		const std::vector<int>& visgroups, const std::vector<int>& groups, goldsrc::SceneAssigner& assigner);
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
