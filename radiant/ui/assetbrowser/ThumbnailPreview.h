#pragma once

#include "imapresource.h"
#include "math/AABB.h"
#include "math/Vector3.h"
#include "wxutil/preview/EntityPreview.h"

class wxImage;

namespace ui
{

class ThumbnailPreview :
    public wxutil::EntityPreview
{
public:
    ThumbnailPreview(wxWindow* parent);

    bool showAsset(const std::string& type, const std::string& name);
    bool captureImage(wxImage& image, int size);

    void setPadding(float padding);

    const Vector3& getAssetViewAngles() const;
    void setAssetViewAngles(const Vector3& angles);

protected:
    bool onPreRender() override;
    AABB getSceneBounds() override;
    void setupInitialViewPosition() override;
    bool canDrawGrid() override;

private:
    bool showPrefab(const std::string& path);
    void showEntityRoot();

    Vector3 _assetViewAngles;
    IMapResourcePtr _prefabResource;
    scene::IMapRootNodePtr _entityRoot;
    AABB _prefabBounds;
    float _padding = 1.1f;
};

}
