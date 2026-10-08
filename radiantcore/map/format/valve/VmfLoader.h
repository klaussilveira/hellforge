#pragma once

#include "map/format/goldsrc/GoldSrcLoader.h"

#include <string>
#include <vector>

namespace map
{

namespace vmf
{

struct Statistics
{
	std::size_t hiddenObjects = 0;
	std::size_t connections = 0;
	std::size_t instances = 0;
	std::vector<std::string> failedInstances;
};

goldsrc::MapData load(const std::string& contents, const std::string& mapPath, Statistics& statistics);

goldsrc::ScanResult scanMap(const std::string& path);

}

}
