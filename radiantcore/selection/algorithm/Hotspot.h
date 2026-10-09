#pragma once

#include "icommandsystem.h"

#include <string>

namespace selection
{

namespace algorithm
{

std::size_t hotspotSelection(bool alt);

void applyHotspotForMaterial(const std::string& material);

void hotspotTextureCmd(const cmd::ArgumentList& args);
void hotspotTextureAltCmd(const cmd::ArgumentList& args);

}

}
