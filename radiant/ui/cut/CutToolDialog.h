#pragma once

#include "icommandsystem.h"
#include "inode.h"
#include "ui/common/GeneratorPreview.h"
#include "wxutil/dialog/Dialog.h"
#include "wxutil/XmlResourceBasedWidget.h"

#include "CutGeometry.h"

#include <wx/timer.h>

#include <string>
#include <vector>

namespace ui
{

class CutToolDialog : public wxutil::Dialog, private wxutil::XmlResourceBasedWidget
{
private:
    std::vector<cut::CutSource> _sources;
    wxTimer _regenerateTimer;

public:
    CutToolDialog(const std::vector<cut::CutSource>& sources);
    ~CutToolDialog() override;

    void commitToMap();
    void clearPreview();
    std::size_t totalCuts();

    static void Show(const cmd::ArgumentList& args);
    static void RunPreset(const cmd::ArgumentList& args);

private:
    void onParameterChanged(wxCommandEvent& ev);
    void onRegenerateTimer(wxTimerEvent& ev);
    void onBrowseMaterial(wxCommandEvent& ev);
    cut::CutParams collectParams();
    void regenerate();
    void updateControlSensitivity();
    void updateCutCount();
    double getNumericValue(const std::string& widgetName, double minimum);
};

} // namespace ui
