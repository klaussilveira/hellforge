#pragma once

#include "DoomMapData.h"

#include <vector>

namespace map
{

namespace doom
{

class UdmfLoader
{
public:
	static DoomMapData load(const std::vector<char>& textMap);
};

}

}
