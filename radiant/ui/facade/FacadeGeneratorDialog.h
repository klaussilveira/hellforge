#pragma once

#include "icommandsystem.h"
#include "inode.h"
#include "math/AABB.h"
#include "ui/common/GeneratorPreview.h"
#include "wxutil/dialog/Dialog.h"
#include "wxutil/XmlResourceBasedWidget.h"

#include "FacadeGeometry.h"

#include <wx/timer.h>

namespace ui
{
class FacadeGeneratorDialog : public wxutil::Dialog, private wxutil::XmlResourceBasedWidget
{
private:
    bool _fromPatch;
    AABB _bounds;
    facade::FacadePath _patchPath;
    scene::INodePtr _parent;
    GeneratorPreview _preview;
    wxTimer _regenerateTimer;
    bool _updating;

public:
    FacadeGeneratorDialog(bool fromPatch, const AABB& bounds,
                          const facade::FacadePath& patchPath, const scene::INodePtr& parent);

    GeneratorPreview& getPreview();
    void commitToMap();

    static void Show(const cmd::ArgumentList& args);

private:
    void onParameterChanged(wxCommandEvent& ev);
    void onPresetChanged(wxCommandEvent& ev);
    void onRegenerateTimer(wxTimerEvent& ev);
    void applyPreset(int preset);
    void updateControlSensitivity();
    facade::FacadeParams collectParams();
    facade::FacadePath collectPath(double wallThickness);
    void generateInto();
    void regenerate();
    double getNumericValue(const std::string& widgetName, double minimum);
    void setNumericValue(const std::string& widgetName, double value);
};

} // namespace ui
