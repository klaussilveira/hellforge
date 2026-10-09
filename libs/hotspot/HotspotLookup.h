#pragma once

#include "HotspotSheet.h"

#include "ifilesystem.h"
#include "igame.h"
#include "iimage.h"
#include "imap.h"
#include "ishaders.h"
#include "itextstream.h"
#include "os/dir.h"
#include "os/path.h"
#include "stream/ScopedArchiveBuffer.h"

#include <fstream>
#include <optional>
#include <string>
#include <vector>

namespace hotspot
{

const char* const MAP_PROPERTY_PREFIX = "hotspot_rect:";
const char* const RECT_EXTENSION = ".rect";
const char* const RKEY_MERGE_COPLANAR = "user/ui/hotspot/mergeCoplanar";
const char* const RKEY_ALLOW_ROTATION = "user/ui/hotspot/allowRotation";

struct LoadedSheet
{
    Sheet sheet;
    std::string path;
};

inline std::string getEditorImagePath(const std::string& material)
{
    if (!GlobalMaterialManager().materialExists(material))
    {
        return {};
    }

    auto expression = GlobalMaterialManager().getMaterial(material)->getEditorImageExpression();

    if (!expression)
    {
        return {};
    }

    auto path = expression->getExpressionString();

    if (path.find('(') != std::string::npos)
    {
        return {};
    }

    return os::removeExtension(os::standardPath(path));
}

inline std::string getMapOverridePath(const std::string& material)
{
    auto root = GlobalMapModule().getRoot();

    return root ? root->getProperty(MAP_PROPERTY_PREFIX + material) : std::string();
}

inline std::vector<std::string> getCandidatePaths(const std::string& material)
{
    std::vector<std::string> paths;

    if (auto overridePath = getMapOverridePath(material); !overridePath.empty())
    {
        paths.push_back(overridePath);
    }

    paths.push_back(material + RECT_EXTENSION);

    if (auto image = getEditorImagePath(material); !image.empty() && image != material)
    {
        paths.push_back(image + RECT_EXTENSION);
    }

    return paths;
}

inline std::optional<Sheet> loadSheetFile(const std::string& path)
{
    auto file = GlobalFileSystem().openFile(path);

    if (!file)
    {
        return std::nullopt;
    }

    archive::ScopedArchiveBuffer buffer(*file);

    try
    {
        return parse(std::string(reinterpret_cast<const char*>(buffer.buffer), buffer.length));
    }
    catch (const parser::ParseException& ex)
    {
        rWarning() << "Failed to parse hotspot rect file " << path << ": " << ex.what() << std::endl;
        return std::nullopt;
    }
}

inline bool getImageSize(const std::string& material, int& width, int& height)
{
    auto imagePath = getEditorImagePath(material);

    if (imagePath.empty())
    {
        return false;
    }

    auto image = GlobalImageLoader().imageFromVFS(imagePath);

    if (!image)
    {
        return false;
    }

    width = static_cast<int>(image->getWidth());
    height = static_cast<int>(image->getHeight());

    return width > 0 && height > 0;
}

inline std::optional<LoadedSheet> findSheet(const std::string& material)
{
    for (const auto& path : getCandidatePaths(material))
    {
        auto sheet = loadSheetFile(path);

        if (!sheet)
        {
            continue;
        }

        if ((sheet->width <= 0 || sheet->height <= 0) && !getImageSize(material, sheet->width, sheet->height))
        {
            rWarning() << "Hotspot rect file " << path << " has no resolution and the image size of "
                << material << " is unknown" << std::endl;
            return std::nullopt;
        }

        return LoadedSheet{ *sheet, path };
    }

    return std::nullopt;
}

inline std::string getMapOnlyPath(const std::string& material)
{
    auto mapName = os::removeExtension(os::getFilename(GlobalMapModule().getMapName()));

    return "hotspots/" + mapName + "/" + material + RECT_EXTENSION;
}

inline std::string getAbsoluteWritePath(const std::string& relativePath)
{
    return os::standardPathWithSlash(GlobalGameManager().getModPath()) + relativePath;
}

inline bool saveSheetFile(const Sheet& sheet, const std::string& relativePath)
{
    auto fullPath = getAbsoluteWritePath(relativePath);

    os::makeDirectory(os::getDirectory(fullPath));

    std::ofstream stream(fullPath, std::ios::binary);

    if (!stream)
    {
        rError() << "Could not open " << fullPath << " for writing" << std::endl;
        return false;
    }

    stream << writeText(sheet);

    return stream.good();
}

}
