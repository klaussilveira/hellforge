#include "ThumbnailViewStore.h"

#include "iregistry.h"
#include "string/convert.h"

namespace ui
{

namespace
{

const std::string RKEY_THUMBNAIL_VIEWS = "user/ui/assetBrowser/thumbnailViews";

}

ThumbnailViewStore::ThumbnailViewStore()
{
    for (const auto& node : GlobalRegistry().findXPath(RKEY_THUMBNAIL_VIEWS + "//view"))
    {
        auto asset = node.getAttributeValue("asset");

        if (asset.empty()) continue;

        _views[asset] = Vector3(
            string::convert<double>(node.getAttributeValue("pitch")),
            string::convert<double>(node.getAttributeValue("yaw")), 0);
    }
}

const Vector3* ThumbnailViewStore::find(const std::string& key) const
{
    auto found = _views.find(key);

    return found != _views.end() ? &found->second : nullptr;
}

void ThumbnailViewStore::set(const std::string& key, const Vector3& angles)
{
    _views[key] = angles;

    save();
}

void ThumbnailViewStore::remove(const std::string& key)
{
    if (_views.erase(key) == 0) return;

    save();
}

void ThumbnailViewStore::save() const
{
    GlobalRegistry().deleteXPath(RKEY_THUMBNAIL_VIEWS + "//view");

    auto root = GlobalRegistry().createKey(RKEY_THUMBNAIL_VIEWS);

    for (const auto& [key, angles] : _views)
    {
        auto node = root.createChild("view");

        node.setAttributeValue("asset", key);
        node.setAttributeValue("pitch", string::to_string(angles[0]));
        node.setAttributeValue("yaw", string::to_string(angles[1]));
    }
}

}
