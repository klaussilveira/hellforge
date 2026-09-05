#pragma once

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
    void setupInitialViewPosition() override;
    bool canDrawGrid() override;

private:
    Vector3 _assetViewAngles;
    float _padding = 1.1f;
};

}
