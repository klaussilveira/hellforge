#include "PencilToolPanel.h"

#include <wx/checkbox.h>
#include <wx/sizer.h>
#include <wx/settings.h>
#include <wx/spinctrl.h>
#include <wx/stattext.h>

#include "i18n.h"
#include "iselection.h"
#include "ui/ieventmanager.h"
#include "ui/imainframe.h"
#include "ui/istatusbarmanager.h"
#include "registry/Widgets.h"
#include "xyview/tools/PencilTool.h"

namespace ui
{

namespace
{
    const char* const RKEY_PENCIL_SMOOTH = "user/ui/penciltool/smooth";
    const char* const RKEY_PENCIL_SMOOTHING = "user/ui/penciltool/smoothing";
}

PencilToolPanel::PencilToolPanel(wxWindow* parent) :
    DockablePanel(parent),
    _smoothCheck(nullptr),
    _smoothingCtrl(nullptr)
{
    populateWindow();
    pushToSettings();

    _settingsChangedConnection = PencilToolSettings::Instance().signal_settingsChanged.connect(
        sigc::mem_fun(*this, &PencilToolPanel::pullFromSettings));
}

PencilToolPanel::~PencilToolPanel()
{
    _settingsChangedConnection.disconnect();

    PencilToolSettings::Instance().active = false;
}

void PencilToolPanel::onPanelActivated()
{
    GlobalSelectionSystem().setSelectedAll(false);

    pushToSettings();
    PencilToolSettings::Instance().active = true;

    GlobalEventManager().setToggled("TogglePencilMode", true);

    GlobalStatusBarManager().setText("Commands",
        _("Pencil Tool: drag to draw a curve, hold CTRL for a straight line"));
}

void PencilToolPanel::onPanelDeactivated()
{
    PencilToolSettings::Instance().active = false;

    GlobalEventManager().setToggled("TogglePencilMode", false);
    GlobalMainFrame().updateAllWindows();
}

void PencilToolPanel::populateWindow()
{
    SetSizer(new wxBoxSizer(wxVERTICAL));

    auto* gridSizer = new wxFlexGridSizer(2, 2, 6, 12);
    gridSizer->AddGrowableCol(1);

    gridSizer->Add(new wxStaticText(this, wxID_ANY, _("Smooth strokes:")), 0, wxALIGN_CENTER_VERTICAL);
    _smoothCheck = new wxCheckBox(this, wxID_ANY, "");
    _smoothCheck->SetValue(true);
    registry::bindWidget(_smoothCheck, RKEY_PENCIL_SMOOTH);
    _smoothCheck->Bind(wxEVT_CHECKBOX, &PencilToolPanel::onCheckBoxChange, this);
    gridSizer->Add(_smoothCheck, 1, wxEXPAND);

    gridSizer->Add(new wxStaticText(this, wxID_ANY, _("Smoothing:")), 0, wxALIGN_CENTER_VERTICAL);
    _smoothingCtrl = new wxSpinCtrlDouble(this, wxID_ANY);
    _smoothingCtrl->SetRange(0.5, 128.0);
    _smoothingCtrl->SetValue(8.0);
    _smoothingCtrl->SetIncrement(1.0);
    _smoothingCtrl->SetDigits(1);
    _smoothingCtrl->SetToolTip(_("Maximum deviation in units the smoothed curve may take from the drawn stroke"));
    registry::bindWidget(_smoothingCtrl, RKEY_PENCIL_SMOOTHING);
    _smoothingCtrl->Bind(wxEVT_SPINCTRLDOUBLE, &PencilToolPanel::onSmoothingChange, this);
    gridSizer->Add(_smoothingCtrl, 1, wxEXPAND);

    GetSizer()->Add(gridSizer, 0, wxEXPAND | wxALL, 12);

    auto* helpText = new wxStaticText(this, wxID_ANY,
        _("Drag in the XY or camera view to draw a curve.\nHold CTRL while drawing to force a straight line.\nSmoothing off keeps the stroke exactly as drawn."));
    helpText->SetForegroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_GRAYTEXT));
    GetSizer()->Add(helpText, 0, wxALL, 12);
}

void PencilToolPanel::pushToSettings()
{
    auto& settings = PencilToolSettings::Instance();

    settings.smooth = _smoothCheck->GetValue();
    settings.smoothing = _smoothingCtrl->GetValue();
}

void PencilToolPanel::pullFromSettings()
{
    const auto& settings = PencilToolSettings::Instance();

    _smoothCheck->SetValue(settings.smooth);
    _smoothingCtrl->SetValue(settings.smoothing);
}

void PencilToolPanel::onSmoothingChange(wxSpinDoubleEvent& ev)
{
    pushToSettings();
    ev.Skip();
}

void PencilToolPanel::onCheckBoxChange(wxCommandEvent& ev)
{
    pushToSettings();
    ev.Skip();
}

}
