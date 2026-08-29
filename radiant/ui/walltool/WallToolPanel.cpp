#include "WallToolPanel.h"

#include <wx/sizer.h>
#include <wx/stattext.h>
#include <wx/spinctrl.h>
#include <wx/textctrl.h>
#include <wx/bmpbuttn.h>
#include <wx/choice.h>

#include "i18n.h"
#include "iselection.h"
#include "ui/ieventmanager.h"
#include "ui/imainframe.h"
#include "ui/istatusbarmanager.h"
#include "irender.h"
#include "registry/Widgets.h"
#include "ui/materials/MaterialChooser.h"
#include "ui/materials/MaterialSelector.h"
#include "wxutil/PickerButton.h"
#include "xyview/tools/WallTool.h"
#include "WallCursorPreview.h"

namespace ui
{

namespace
{
    const char* const RKEY_WALL_HEIGHT = "user/ui/walltool/wallHeight";
    const char* const RKEY_WALL_THICKNESS = "user/ui/walltool/wallThickness";
    const char* const RKEY_WALL_MATERIAL = "user/ui/walltool/wallMaterial";
    const char* const RKEY_FLOOR_THICKNESS = "user/ui/walltool/floorThickness";
    const char* const RKEY_FLOOR_MATERIAL = "user/ui/walltool/floorMaterial";
    const char* const RKEY_ROOF_TYPE = "user/ui/walltool/roofType";
    const char* const RKEY_ROOF_PITCH = "user/ui/walltool/roofPitch";
    const char* const RKEY_ROOF_MATERIAL = "user/ui/walltool/roofMaterial";
}

WallToolPanel::WallToolPanel(wxWindow* parent) :
    DockablePanel(parent),
    _heightCtrl(nullptr),
    _thicknessCtrl(nullptr),
    _floorThicknessCtrl(nullptr),
    _roofPitchCtrl(nullptr),
    _roofTypeChoice(nullptr),
    _wallMaterialEntry(nullptr),
    _floorMaterialEntry(nullptr),
    _roofMaterialEntry(nullptr),
    _preview(new WallCursorPreview)
{
    populateWindow();
}

WallToolPanel::~WallToolPanel()
{
    if (_previewAttached)
    {
        GlobalRenderSystem().detachRenderable(*_preview);
        _previewAttached = false;
    }
}

void WallToolPanel::onPanelActivated()
{
    GlobalSelectionSystem().setSelectedAll(false);

    pushToSettings();
    WallToolSettings::Instance().active = true;

    if (!_previewAttached)
    {
        GlobalRenderSystem().attachRenderable(*_preview);
        _previewAttached = true;
    }

    GlobalEventManager().setToggled("ToggleWallMode", true);

    GlobalStatusBarManager().setText("Commands",
        _("Wall Tool: drag to draw, ALT+Click to chain, CTRL+Click to erase"));
}

void WallToolPanel::onPanelDeactivated()
{
    if (_previewAttached)
    {
        _preview->clear();
        GlobalRenderSystem().detachRenderable(*_preview);
        _previewAttached = false;
    }

    auto& settings = WallToolSettings::Instance();
    settings.active = false;
    settings.hoverValid = false;
    settings.hoverConnected = false;
    settings.segmentPreviewValid = false;

    GlobalEventManager().setToggled("ToggleWallMode", false);
    GlobalMainFrame().updateAllWindows();
}

wxSpinCtrlDouble* WallToolPanel::makeSpin(double min, double max, double value, double increment)
{
    auto* spin = new wxSpinCtrlDouble(this, wxID_ANY);
    spin->SetRange(min, max);
    spin->SetValue(value);
    spin->SetIncrement(increment);
    spin->SetDigits(0);
    spin->Bind(wxEVT_SPINCTRLDOUBLE, &WallToolPanel::onSpinChange, this);

    return spin;
}

void WallToolPanel::addMaterialRow(wxFlexGridSizer* sizer, const wxString& label,
    wxTextCtrl*& entry, const std::string& defaultValue, const std::string& registryKey)
{
    sizer->Add(new wxStaticText(this, wxID_ANY, label), 0, wxALIGN_CENTER_VERTICAL);

    auto* rowSizer = new wxBoxSizer(wxHORIZONTAL);

    entry = new wxTextCtrl(this, wxID_ANY, defaultValue);
    entry->SetMinSize(wxSize(120, -1));
    registry::bindWidget(entry, registryKey);
    entry->Bind(wxEVT_TEXT, &WallToolPanel::onMaterialChange, this);

    auto* browseButton = wxutil::PickerButton(this);

    wxTextCtrl* target = entry;
    browseButton->Bind(wxEVT_BUTTON, [this, target](wxCommandEvent&)
    {
        auto* chooser = new MaterialChooser(this, MaterialSelector::TextureFilter::Regular, target);
        chooser->ShowModal();
        chooser->Destroy();
    });

    rowSizer->Add(entry, 1, wxEXPAND | wxRIGHT, 4);
    rowSizer->Add(browseButton, 0);

    sizer->Add(rowSizer, 1, wxEXPAND);
}

void WallToolPanel::populateWindow()
{
    SetSizer(new wxBoxSizer(wxVERTICAL));

    auto* gridSizer = new wxFlexGridSizer(8, 2, 6, 12);
    gridSizer->AddGrowableCol(1);

    gridSizer->Add(new wxStaticText(this, wxID_ANY, _("Wall height:")), 0, wxALIGN_CENTER_VERTICAL);
    _heightCtrl = makeSpin(1.0, 4096.0, 128.0, 8.0);
    registry::bindWidget(_heightCtrl, RKEY_WALL_HEIGHT);
    gridSizer->Add(_heightCtrl, 1, wxEXPAND);

    gridSizer->Add(new wxStaticText(this, wxID_ANY, _("Wall thickness:")), 0, wxALIGN_CENTER_VERTICAL);
    _thicknessCtrl = makeSpin(1.0, 512.0, 8.0, 4.0);
    registry::bindWidget(_thicknessCtrl, RKEY_WALL_THICKNESS);
    gridSizer->Add(_thicknessCtrl, 1, wxEXPAND);

    addMaterialRow(gridSizer, _("Wall material:"), _wallMaterialEntry, "_default", RKEY_WALL_MATERIAL);

    gridSizer->Add(new wxStaticText(this, wxID_ANY, _("Floor thickness:")), 0, wxALIGN_CENTER_VERTICAL);
    _floorThicknessCtrl = makeSpin(1.0, 512.0, 8.0, 4.0);
    registry::bindWidget(_floorThicknessCtrl, RKEY_FLOOR_THICKNESS);
    gridSizer->Add(_floorThicknessCtrl, 1, wxEXPAND);

    addMaterialRow(gridSizer, _("Floor material:"), _floorMaterialEntry, "_default", RKEY_FLOOR_MATERIAL);

    gridSizer->Add(new wxStaticText(this, wxID_ANY, _("Roof type:")), 0, wxALIGN_CENTER_VERTICAL);
    _roofTypeChoice = new wxChoice(this, wxID_ANY);
    _roofTypeChoice->Append(_("Flat"));
    _roofTypeChoice->Append(_("Shed"));
    _roofTypeChoice->Append(_("Gabled"));

    int roofType = registry::getValue<int>(RKEY_ROOF_TYPE);
    _roofTypeChoice->SetSelection(roofType >= 0 && roofType <= 2 ? roofType : 0);
    _roofTypeChoice->Bind(wxEVT_CHOICE, &WallToolPanel::onRoofTypeChange, this);
    gridSizer->Add(_roofTypeChoice, 1, wxEXPAND);

    gridSizer->Add(new wxStaticText(this, wxID_ANY, _("Roof pitch:")), 0, wxALIGN_CENTER_VERTICAL);
    _roofPitchCtrl = makeSpin(5.0, 75.0, 30.0, 5.0);
    _roofPitchCtrl->SetToolTip(_("Roof slope angle in degrees"));
    registry::bindWidget(_roofPitchCtrl, RKEY_ROOF_PITCH);
    gridSizer->Add(_roofPitchCtrl, 1, wxEXPAND);

    addMaterialRow(gridSizer, _("Roof material:"), _roofMaterialEntry, "_default", RKEY_ROOF_MATERIAL);

    GetSizer()->Add(gridSizer, 0, wxEXPAND | wxALL, 12);

    auto* helpText = new wxStaticText(this, wxID_ANY,
        _("Drag in the XY or camera view to draw walls.\nALT+Click chains from the last endpoint.\nCTRL+Click or drag erases walls.\nClosing a room adds floor and roof."));
    helpText->SetForegroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_GRAYTEXT));
    GetSizer()->Add(helpText, 0, wxALL, 12);

    updateRoofPitchEnabled();
}

void WallToolPanel::pushToSettings()
{
    auto& settings = WallToolSettings::Instance();

    settings.wallHeight = _heightCtrl->GetValue();
    settings.wallThickness = _thicknessCtrl->GetValue();
    settings.floorThickness = _floorThicknessCtrl->GetValue();
    settings.roofPitch = _roofPitchCtrl->GetValue();
    settings.roofType = static_cast<WallRoofType>(_roofTypeChoice->GetSelection());
    settings.wallMaterial = _wallMaterialEntry->GetValue().ToStdString();
    settings.floorMaterial = _floorMaterialEntry->GetValue().ToStdString();
    settings.roofMaterial = _roofMaterialEntry->GetValue().ToStdString();
}

void WallToolPanel::updateRoofPitchEnabled()
{
    _roofPitchCtrl->Enable(_roofTypeChoice->GetSelection() != 0);
}

void WallToolPanel::onSpinChange(wxSpinDoubleEvent& ev)
{
    pushToSettings();
    ev.Skip();
}

void WallToolPanel::onRoofTypeChange(wxCommandEvent& ev)
{
    registry::setValue(RKEY_ROOF_TYPE, _roofTypeChoice->GetSelection());
    pushToSettings();
    updateRoofPitchEnabled();
    ev.Skip();
}

void WallToolPanel::onMaterialChange(wxCommandEvent& ev)
{
    pushToSettings();
    ev.Skip();
}

}
