#include "BuildingGeneratorDialog.h"
#include "BuildingGeometry.h"

#include "i18n.h"
#include "ui/imainframe.h"
#include "imap.h"
#include "iselection.h"
#include "ibrush.h"
#include "ishaderclipboard.h"
#include "iundo.h"

#include "string/convert.h"
#include "selectionlib.h"
#include "scenelib.h"
#include "shaderlib.h"
#include "math/Vector3.h"
#include "math/AABB.h"

#include <wx/stattext.h>
#include <wx/textctrl.h>
#include <wx/spinctrl.h>
#include <wx/choice.h>
#include <wx/checkbox.h>
#include <wx/button.h>
#include <wx/statbox.h>
#include <wx/msgdlg.h>

#include "ui/materials/MaterialChooser.h"
#include "ui/materials/MaterialSelector.h"

namespace
{
const char* const WINDOW_TITLE = N_("Building Generator");

inline std::string getSelectedShader()
{
    auto selectedShader = GlobalShaderClipboard().getShaderName();
    if (selectedShader.empty())
        selectedShader = texdef_name_default();
    return selectedShader;
}

} // anonymous namespace

namespace ui
{

BuildingGeneratorDialog::BuildingGeneratorDialog()
    : Dialog(_(WINDOW_TITLE), GlobalMainFrame().getWxTopLevelWindow()),
      _slantedRoofPanel(nullptr), _aRoofPanel(nullptr), _borderTrimPanel(nullptr)
{
    _dialog->GetSizer()->Add(
        loadNamedPanel(_dialog, "BuildingGeneratorMainPanel"), 1, wxEXPAND | wxALL, 12);

    wxStaticText* topLabel = findNamedObject<wxStaticText>(_dialog, "BuildingGeneratorTopLabel");
    topLabel->SetFont(topLabel->GetFont().Bold());

    // Get panel references for visibility toggling
    _slantedRoofPanel = findNamedObject<wxWindow>(_dialog, "BuildingGeneratorSlantedPanel");
    _aRoofPanel = findNamedObject<wxWindow>(_dialog, "BuildingGeneratorARoofPanel");
    _borderTrimPanel = findNamedObject<wxWindow>(_dialog, "BuildingGeneratorBorderTrimPanel");

    // Bind events
    findNamedObject<wxChoice>(_dialog, "BuildingGeneratorRoofType")
        ->Bind(wxEVT_CHOICE, &BuildingGeneratorDialog::onRoofTypeChanged, this);

    findNamedObject<wxChoice>(_dialog, "BuildingGeneratorWindowMode")
        ->Bind(wxEVT_CHOICE, &BuildingGeneratorDialog::onWindowModeChanged, this);

    findNamedObject<wxButton>(_dialog, "BuildingGeneratorBrowseWallMaterial")
        ->Bind(wxEVT_BUTTON, &BuildingGeneratorDialog::onBrowseWallMaterial, this);

    findNamedObject<wxButton>(_dialog, "BuildingGeneratorBrowseTrimMaterial")
        ->Bind(wxEVT_BUTTON, &BuildingGeneratorDialog::onBrowseTrimMaterial, this);

    findNamedObject<wxButton>(_dialog, "BuildingGeneratorBrowseFrameMaterial")
        ->Bind(wxEVT_BUTTON, &BuildingGeneratorDialog::onBrowseWindowFrameMaterial, this);

    auto shader = getSelectedShader();
    findNamedObject<wxTextCtrl>(_dialog, "BuildingGeneratorWallMaterial")->SetValue(shader);
    findNamedObject<wxTextCtrl>(_dialog, "BuildingGeneratorTrimMaterial")->SetValue(shader);
    findNamedObject<wxTextCtrl>(_dialog, "BuildingGeneratorFrameMaterial")->SetValue(shader);

    updateControlVisibility();
}

void BuildingGeneratorDialog::onRoofTypeChanged(wxCommandEvent& ev)
{
    updateControlVisibility();
    _dialog->Layout();
    _dialog->Fit();
}

void BuildingGeneratorDialog::onWindowModeChanged(wxCommandEvent& ev)
{
    bool manual = getWindowMode() == 1;
    findNamedObject<wxWindow>(_dialog, "BuildingGeneratorWindowsPerWallLabel")->Show(manual);
    findNamedObject<wxWindow>(_dialog, "BuildingGeneratorWindowsPerWall")->Show(manual);
    _dialog->Layout();
    _dialog->Fit();
}

void BuildingGeneratorDialog::onBrowseWallMaterial(wxCommandEvent& ev)
{
    wxTextCtrl* entry = findNamedObject<wxTextCtrl>(_dialog, "BuildingGeneratorWallMaterial");
    MaterialChooser chooser(_dialog, MaterialSelector::TextureFilter::Regular, entry);
    chooser.ShowModal();
}

void BuildingGeneratorDialog::onBrowseTrimMaterial(wxCommandEvent& ev)
{
    wxTextCtrl* entry = findNamedObject<wxTextCtrl>(_dialog, "BuildingGeneratorTrimMaterial");
    MaterialChooser chooser(_dialog, MaterialSelector::TextureFilter::Regular, entry);
    chooser.ShowModal();
}

void BuildingGeneratorDialog::onBrowseWindowFrameMaterial(wxCommandEvent& ev)
{
    wxTextCtrl* entry = findNamedObject<wxTextCtrl>(_dialog, "BuildingGeneratorFrameMaterial");
    MaterialChooser chooser(_dialog, MaterialSelector::TextureFilter::Regular, entry);
    chooser.ShowModal();
}

void BuildingGeneratorDialog::updateControlVisibility()
{
    int roofType = getRoofType();
    if (_borderTrimPanel) _borderTrimPanel->Show(roofType == 1);
    if (_slantedRoofPanel) _slantedRoofPanel->Show(roofType == 2);
    if (_aRoofPanel) _aRoofPanel->Show(roofType == 3);

    bool manual = getWindowMode() == 1;
    findNamedObject<wxWindow>(_dialog, "BuildingGeneratorWindowsPerWallLabel")->Show(manual);
    findNamedObject<wxWindow>(_dialog, "BuildingGeneratorWindowsPerWall")->Show(manual);
}

// Getters

int BuildingGeneratorDialog::getFloorCount()
{
    return findNamedObject<wxSpinCtrl>(_dialog, "BuildingGeneratorFloorCount")->GetValue();
}

float BuildingGeneratorDialog::getFloorHeight()
{
    return string::convert<float>(
        findNamedObject<wxTextCtrl>(_dialog, "BuildingGeneratorFloorHeight")->GetValue().ToStdString(), 128.0f);
}

float BuildingGeneratorDialog::getFloorThickness()
{
    return string::convert<float>(
        findNamedObject<wxTextCtrl>(_dialog, "BuildingGeneratorFloorThickness")->GetValue().ToStdString(), 8.0f);
}

float BuildingGeneratorDialog::getTrimHeight()
{
    return string::convert<float>(
        findNamedObject<wxTextCtrl>(_dialog, "BuildingGeneratorTrimHeight")->GetValue().ToStdString(), 4.0f);
}

float BuildingGeneratorDialog::getWallThickness()
{
    return string::convert<float>(
        findNamedObject<wxTextCtrl>(_dialog, "BuildingGeneratorWallThickness")->GetValue().ToStdString(), 8.0f);
}

int BuildingGeneratorDialog::getWindowMode()
{
    return findNamedObject<wxChoice>(_dialog, "BuildingGeneratorWindowMode")->GetSelection();
}

int BuildingGeneratorDialog::getWindowsPerWall()
{
    return findNamedObject<wxSpinCtrl>(_dialog, "BuildingGeneratorWindowsPerWall")->GetValue();
}

float BuildingGeneratorDialog::getWindowWidth()
{
    return string::convert<float>(
        findNamedObject<wxTextCtrl>(_dialog, "BuildingGeneratorWindowWidth")->GetValue().ToStdString(), 32.0f);
}

float BuildingGeneratorDialog::getWindowHeight()
{
    return string::convert<float>(
        findNamedObject<wxTextCtrl>(_dialog, "BuildingGeneratorWindowHeight")->GetValue().ToStdString(), 48.0f);
}

float BuildingGeneratorDialog::getWindowSillHeight()
{
    return string::convert<float>(
        findNamedObject<wxTextCtrl>(_dialog, "BuildingGeneratorWindowSillHeight")->GetValue().ToStdString(), 24.0f);
}

float BuildingGeneratorDialog::getWindowInset()
{
    return string::convert<float>(
        findNamedObject<wxTextCtrl>(_dialog, "BuildingGeneratorWindowInset")->GetValue().ToStdString(), 2.0f);
}

int BuildingGeneratorDialog::getRoofType()
{
    return findNamedObject<wxChoice>(_dialog, "BuildingGeneratorRoofType")->GetSelection();
}

float BuildingGeneratorDialog::getRoofBorderHeight()
{
    return string::convert<float>(
        findNamedObject<wxTextCtrl>(_dialog, "BuildingGeneratorRoofBorderHeight")->GetValue().ToStdString(), 16.0f);
}

float BuildingGeneratorDialog::getRoofSlopeHeight()
{
    return string::convert<float>(
        findNamedObject<wxTextCtrl>(_dialog, "BuildingGeneratorRoofSlopeHeight")->GetValue().ToStdString(), 64.0f);
}

int BuildingGeneratorDialog::getRoofSlopeDirection()
{
    return findNamedObject<wxChoice>(_dialog, "BuildingGeneratorRoofSlopeDirection")->GetSelection();
}

float BuildingGeneratorDialog::getARoofHeight()
{
    return string::convert<float>(
        findNamedObject<wxTextCtrl>(_dialog, "BuildingGeneratorARoofHeight")->GetValue().ToStdString(), 64.0f);
}

int BuildingGeneratorDialog::getARoofDirection()
{
    return findNamedObject<wxChoice>(_dialog, "BuildingGeneratorARoofDirection")->GetSelection();
}

std::string BuildingGeneratorDialog::getWallMaterial()
{
    return findNamedObject<wxTextCtrl>(_dialog, "BuildingGeneratorWallMaterial")->GetValue().ToStdString();
}

std::string BuildingGeneratorDialog::getTrimMaterial()
{
    return findNamedObject<wxTextCtrl>(_dialog, "BuildingGeneratorTrimMaterial")->GetValue().ToStdString();
}

std::string BuildingGeneratorDialog::getWindowFrameMaterial()
{
    return findNamedObject<wxTextCtrl>(_dialog, "BuildingGeneratorFrameMaterial")->GetValue().ToStdString();
}

void BuildingGeneratorDialog::Show(const cmd::ArgumentList& args)
{
    // Require exactly one brush selected
    if (GlobalSelectionSystem().countSelected() != 1)
    {
        wxMessageBox(_("Please select exactly one brush to use as the building footprint."),
            _("Building Generator"), wxOK | wxICON_WARNING);
        return;
    }

    auto selectedNode = GlobalSelectionSystem().ultimateSelected();
    auto* brush = Node_getIBrush(selectedNode);
    if (!brush)
    {
        wxMessageBox(_("The selected object must be a brush."),
            _("Building Generator"), wxOK | wxICON_WARNING);
        return;
    }

    // Get the AABB of the selected brush
    AABB bounds = selectedNode->worldAABB();
    if (!bounds.isValid())
    {
        wxMessageBox(_("Could not determine the bounds of the selected brush."),
            _("Building Generator"), wxOK | wxICON_WARNING);
        return;
    }

    BuildingGeneratorDialog dialog;

    if (dialog.run() != IDialog::RESULT_OK)
        return;

    building::BuildingParams params;
    params.floorCount = dialog.getFloorCount();
    params.floorHeight = dialog.getFloorHeight();
    params.floorThickness = dialog.getFloorThickness();
    params.trimHeight = dialog.getTrimHeight();
    params.wallThickness = dialog.getWallThickness();

    params.windowMode = dialog.getWindowMode();
    params.windowsPerWall = dialog.getWindowsPerWall();
    params.windowWidth = dialog.getWindowWidth();
    params.windowHeight = dialog.getWindowHeight();
    params.windowSillHeight = dialog.getWindowSillHeight();
    params.windowInset = dialog.getWindowInset();

    params.roofType = dialog.getRoofType();
    params.roofBorderHeight = dialog.getRoofBorderHeight();
    params.roofSlopeHeight = dialog.getRoofSlopeHeight();
    params.roofSlopeDirection = dialog.getRoofSlopeDirection();
    params.aRoofHeight = dialog.getARoofHeight();
    params.aRoofDirection = dialog.getARoofDirection();

    params.wallMaterial = dialog.getWallMaterial();
    params.trimMaterial = dialog.getTrimMaterial();
    params.windowFrameMaterial = dialog.getWindowFrameMaterial();

    UndoableCommand undo("buildingGeneratorCreate");

    // Delete the original brush
    scene::INodePtr parent = selectedNode->getParent();
    if (!parent)
        parent = GlobalMapModule().findOrInsertWorldspawn();

    scene::removeNodeFromParent(selectedNode);

    GlobalSelectionSystem().setSelectedAll(false);

    building::generateBuilding(bounds, params, parent);
}

} // namespace ui
