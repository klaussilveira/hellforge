#pragma once

#include "RoadShape.h"

#include "icommandsystem.h"
#include "inode.h"
#include "math/Vector3.h"
#include "ui/common/GeneratorPreview.h"
#include "wxutil/dialog/Dialog.h"
#include "wxutil/XmlResourceBasedWidget.h"

#include <wx/timer.h>

#include <string>
#include <vector>

namespace ui
{

class RoadGeneratorDialog : public wxutil::Dialog, private wxutil::XmlResourceBasedWidget
{
private:
    std::vector<std::vector<Vector3>> _curves;
    scene::INodePtr _parent;
    GeneratorPreview _preview;
    wxTimer _regenerateTimer;

public:
    RoadGeneratorDialog(const std::vector<std::vector<Vector3>>& curves,
                        const scene::INodePtr& parent);

    GeneratorPreview& getPreview();
    void commitToMap();

    static void Show(const cmd::ArgumentList& args);

private:
    void onParameterChanged(wxCommandEvent& ev);
    void onRegenerateTimer(wxTimerEvent& ev);
    void onBrowseMaterial(wxCommandEvent& ev);
    void generateInto();
    void regenerate();
    double getNumericValue(const std::string& widgetName, double minimum);
    void updateControlSensitivity();
    road::RoadParams collectParams();
};

} // namespace ui
