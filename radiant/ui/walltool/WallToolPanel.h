#pragma once

#include "wxutil/DockablePanel.h"

#include <memory>
#include <string>

class wxSpinCtrlDouble;
class wxTextCtrl;
class wxChoice;
class wxFlexGridSizer;
class wxSpinDoubleEvent;

namespace ui
{

class WallCursorPreview;

class WallToolPanel :
    public wxutil::DockablePanel
{
private:
    wxSpinCtrlDouble* _heightCtrl;
    wxSpinCtrlDouble* _thicknessCtrl;
    wxSpinCtrlDouble* _floorThicknessCtrl;
    wxSpinCtrlDouble* _roofPitchCtrl;
    wxChoice* _roofTypeChoice;
    wxTextCtrl* _wallMaterialEntry;
    wxTextCtrl* _floorMaterialEntry;
    wxTextCtrl* _roofMaterialEntry;

    std::unique_ptr<WallCursorPreview> _preview;
    bool _previewAttached = false;

public:
    WallToolPanel(wxWindow* parent);
    ~WallToolPanel() override;

protected:
    void onPanelActivated() override;
    void onPanelDeactivated() override;

private:
    void populateWindow();
    wxSpinCtrlDouble* makeSpin(double min, double max, double value, double increment);
    void addMaterialRow(wxFlexGridSizer* sizer, const wxString& label,
        wxTextCtrl*& entry, const std::string& defaultValue, const std::string& registryKey);
    void pushToSettings();
    void updateRoofPitchEnabled();
    void onSpinChange(wxSpinDoubleEvent& ev);
    void onRoofTypeChange(wxCommandEvent& ev);
    void onMaterialChange(wxCommandEvent& ev);
};

}
