#pragma once

#include "iscript.h"
#include "iscriptinterface.h"
#include "iaasfile.h"

namespace script
{

class AasFileInterface :
	public IScriptInterface
{
public:
	void registerInterface(py::module& scope, py::dict& globals) override;

private:
	std::vector<map::AasFileInfo> getAasFilesForCurrentMap();
	map::IAasFilePtr loadAasFile(const std::string& absolutePath);
};

} // namespace script
