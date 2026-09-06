#pragma once

#include "DoomMapData.h"
#include "WadFile.h"

namespace map
{

namespace doom
{

class DoomBinaryLoader
{
public:
	static DoomMapData load(WadFile& wad, const WadMapGroup& group);
};

}

}
