#pragma once

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
    bool captureImage(wxImage& image);

protected:
    void setupInitialViewPosition() override;
    bool canDrawGrid() override;
};

}
