#include "MapFileManager.h"

#include "i18n.h"
#include "ifiletypes.h"
#include "messages/FileSelectionRequest.h"

namespace map
{

void MapFileManager::registerFileTypes()
{
	GlobalFiletypes().registerPattern(filetype::TYPE_MAP, FileTypePattern(_("Map"), "map", "*.map"));
	GlobalFiletypes().registerPattern(filetype::TYPE_MAP, FileTypePattern(_("Portable Map"), "mapx", "*.mapx"));
	GlobalFiletypes().registerPattern(filetype::TYPE_MAP, FileTypePattern(_("Valve 220"), "map", "*.map", "", "Valve 220"));
	GlobalFiletypes().registerPattern(filetype::TYPE_MAP, FileTypePattern(_("Quake 1"), "map", "*.map", "", "Quake 1"));
	GlobalFiletypes().registerPattern(filetype::TYPE_MAP, FileTypePattern(_("Quake 2"), "map", "*.map", "", "Quake 2"));
	GlobalFiletypes().registerPattern(filetype::TYPE_MAP, FileTypePattern(_("Valve VMF"), "vmf", "*.vmf", "", "Valve VMF"));
	GlobalFiletypes().registerPattern(filetype::TYPE_REGION, FileTypePattern(_("Region"), "reg", "*.reg"));
	GlobalFiletypes().registerPattern(filetype::TYPE_PREFAB, FileTypePattern(_("Portable Prefab"), "pfbx", "*.pfbx"));
	GlobalFiletypes().registerPattern(filetype::TYPE_PREFAB, FileTypePattern(_("Prefab"), "pfb", "*.pfb"));

	GlobalFiletypes().registerPattern(filetype::TYPE_MAP_EXPORT, FileTypePattern(_("Map"), "map", "*.map"));
	GlobalFiletypes().registerPattern(filetype::TYPE_MAP_EXPORT, FileTypePattern(_("Map"), "mapx", "*.mapx"));
}

MapFileSelection MapFileManager::getMapFileSelection(bool open,
										 const std::string& title,
										 const std::string& type,
										 const std::string& defaultFile)
{
	auto mode = open ? 
		radiant::FileSelectionRequest::Mode::Open : 
		radiant::FileSelectionRequest::Mode::Save;

	radiant::FileSelectionRequest request(mode, title, type, defaultFile);
	GlobalRadiantCore().getMessageBus().sendMessage(request);

	MapFileSelection result;

	result.fullPath = request.getResult().fullPath;
	result.mapFormatName = request.getResult().mapFormatName;
	result.mapFormat = GlobalMapFormatManager().getMapFormatByName(request.getResult().mapFormatName);

	return result;
}

} // namespace map
