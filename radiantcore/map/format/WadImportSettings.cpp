#include "WadImportSettings.h"

namespace map
{

std::string WadImportSettings::_mapName;
double WadImportSettings::_scale = WadImportSettings::DEFAULT_SCALE;
double WadImportSettings::_lightSpacing = WadImportSettings::DEFAULT_LIGHT_SPACING;
bool WadImportSettings::_cancelled = false;

}
