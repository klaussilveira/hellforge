#pragma once

#include <functional>
#include <string>

#include <wx/dnd.h>

#include "iinteractiveview.h"
#include "math/Vector3.h"

namespace ui
{

std::string makeAssetDragPayload(const std::string& type, const std::string& name);

class AssetDropTarget :
    public wxTextDropTarget
{
public:
    using FallbackPointFunc = std::function<Vector3(int, int, const Vector3&)>;

    AssetDropTarget(IInteractiveView& view, FallbackPointFunc fallback);

    bool OnDropText(wxCoord x, wxCoord y, const wxString& text) override;

private:
    IInteractiveView& _view;
    FallbackPointFunc _fallback;
};

}
