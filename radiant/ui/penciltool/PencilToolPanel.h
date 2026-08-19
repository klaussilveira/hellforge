#pragma once

#include "wxutil/DockablePanel.h"

#include <sigc++/connection.h>

class wxCheckBox;
class wxSpinCtrlDouble;
class wxSpinDoubleEvent;

namespace ui
{

class PencilToolPanel :
    public wxutil::DockablePanel
{
private:
    wxCheckBox* _smoothCheck;
    wxSpinCtrlDouble* _smoothingCtrl;

    sigc::connection _settingsChangedConnection;

public:
    PencilToolPanel(wxWindow* parent);
    ~PencilToolPanel() override;

protected:
    void onPanelActivated() override;
    void onPanelDeactivated() override;

private:
    void populateWindow();
    void pushToSettings();
    void pullFromSettings();
    void onSmoothingChange(wxSpinDoubleEvent& ev);
    void onCheckBoxChange(wxCommandEvent& ev);
};

}
