#pragma once

#include "icommandsystem.h"
#include "inode.h"
#include "math/AABB.h"
#include "wxutil/dialog/Dialog.h"
#include "wxutil/XmlResourceBasedWidget.h"

#include <vector>

namespace ui
{

class RoofGeneratorDialog : public wxutil::Dialog, private wxutil::XmlResourceBasedWidget
{
private:
    std::vector<AABB> _walls;
    AABB _footprint;
    scene::INodePtr _parent;
    std::vector<scene::INodePtr> _generated;

public:
    RoofGeneratorDialog(const std::vector<AABB>& walls, const AABB& footprint,
                        const scene::INodePtr& parent);

    void selectGenerated();
    void removeGenerated();

    static void Show(const cmd::ArgumentList& args);

private:
    void onParameterChanged(wxCommandEvent& ev);
    void onBrowseMaterial(wxCommandEvent& ev);
    void regenerate();
    double getNumericValue(const std::string& widgetName, double minimum);
    void updateControlSensitivity();
};

} // namespace ui
