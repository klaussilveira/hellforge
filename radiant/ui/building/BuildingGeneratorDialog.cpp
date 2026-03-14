#include "BuildingGeneratorDialog.h"
#include "BuildingGeometry.h"

#include "i18n.h"
#include "ui/imainframe.h"
#include "imap.h"
#include "iselection.h"
#include "icameraview.h"
#include "ishaderclipboard.h"
#include "iundo.h"

#include "string/convert.h"
#include "selectionlib.h"
#include "scenelib.h"
#include "shaderlib.h"
#include "math/Vector3.h"

#include <wx/stattext.h>
#include <wx/textctrl.h>
#include <wx/spinctrl.h>
#include <wx/choice.h>
#include <wx/button.h>
#include <wx/checkbox.h>
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

Vector3 getSpawnPosition()
{
    try
    {
        return GlobalCameraManager().getActiveView().getCameraOrigin();
    }
    catch (const std::runtime_error&) {}
    return Vector3(0, 0, 0);
}

} // anonymous namespace

namespace ui
{

BuildingGeneratorDialog::BuildingGeneratorDialog(bool hasBrushSelection, double defaultFloorHeight)
    : Dialog(_(WINDOW_TITLE), GlobalMainFrame().getWxTopLevelWindow()),
      _dimensionsPanel(nullptr),
      _floorHeightPanel(nullptr),
      _windowParamsPanel(nullptr),
      _windowCountPanel(nullptr),
      _cornerExtrudePanel(nullptr),
      _roofHeightPanel(nullptr),
      _roofBorderPanel(nullptr),
      _hasBrushSelection(hasBrushSelection)
{
    _dialog->GetSizer()->Add(
        loadNamedPanel(_dialog, "BuildingGeneratorMainPanel"), 1, wxEXPAND | wxALL, 12);

    wxStaticText* topLabel = findNamedObject<wxStaticText>(_dialog, "BuildingGeneratorTopLabel");
    topLabel->SetFont(topLabel->GetFont().Bold());

    _dimensionsPanel = findNamedObject<wxWindow>(_dialog, "BuildingGeneratorDimensionsPanel");
    _floorHeightPanel = findNamedObject<wxWindow>(_dialog, "BuildingGeneratorFloorHeightPanel");
    _windowParamsPanel = findNamedObject<wxWindow>(_dialog, "BuildingGeneratorWindowParamsPanel");
    _windowCountPanel = findNamedObject<wxWindow>(_dialog, "BuildingGeneratorWindowCountPanel");
    _cornerExtrudePanel = findNamedObject<wxWindow>(_dialog, "BuildingGeneratorCornerExtrudePanel");
    _roofHeightPanel = findNamedObject<wxWindow>(_dialog, "BuildingGeneratorRoofHeightPanel");
    _roofBorderPanel = findNamedObject<wxWindow>(_dialog, "BuildingGeneratorRoofBorderPanel");

    findNamedObject<wxChoice>(_dialog, "BuildingGeneratorFloorHeightMode")
        ->Bind(wxEVT_CHOICE, &BuildingGeneratorDialog::onFloorHeightModeChanged, this);

    findNamedObject<wxChoice>(_dialog, "BuildingGeneratorWindowMode")
        ->Bind(wxEVT_CHOICE, &BuildingGeneratorDialog::onWindowModeChanged, this);

    findNamedObject<wxCheckBox>(_dialog, "BuildingGeneratorCornerColumns")
        ->Bind(wxEVT_CHECKBOX, &BuildingGeneratorDialog::onCornerColumnsChanged, this);

    findNamedObject<wxChoice>(_dialog, "BuildingGeneratorRoofType")
        ->Bind(wxEVT_CHOICE, &BuildingGeneratorDialog::onRoofTypeChanged, this);

    findNamedObject<wxButton>(_dialog, "BuildingGeneratorBrowseWallMaterial")
        ->Bind(wxEVT_BUTTON, &BuildingGeneratorDialog::onBrowseWallMaterial, this);

    findNamedObject<wxButton>(_dialog, "BuildingGeneratorBrowseTrimMaterial")
        ->Bind(wxEVT_BUTTON, &BuildingGeneratorDialog::onBrowseTrimMaterial, this);

    std::string shader = getSelectedShader();
    findNamedObject<wxTextCtrl>(_dialog, "BuildingGeneratorWallMaterial")->SetValue(shader);
    findNamedObject<wxTextCtrl>(_dialog, "BuildingGeneratorTrimMaterial")->SetValue(shader);

    int fh = static_cast<int>(defaultFloorHeight);
    findNamedObject<wxTextCtrl>(_dialog, "BuildingGeneratorFloorHeight")
        ->SetValue(string::to_string(fh));

    updateControlVisibility();
}

void BuildingGeneratorDialog::onFloorHeightModeChanged(wxCommandEvent& ev)
{
    updateControlVisibility();
    _dialog->Layout();
    _dialog->Fit();
}

void BuildingGeneratorDialog::onWindowModeChanged(wxCommandEvent& ev)
{
    updateControlVisibility();
    _dialog->Layout();
    _dialog->Fit();
}

void BuildingGeneratorDialog::onRoofTypeChanged(wxCommandEvent& ev)
{
    updateControlVisibility();
    _dialog->Layout();
    _dialog->Fit();
}

void BuildingGeneratorDialog::onCornerColumnsChanged(wxCommandEvent& ev)
{
    updateControlVisibility();
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

void BuildingGeneratorDialog::updateControlVisibility()
{
    if (_dimensionsPanel)
        _dimensionsPanel->Show(!_hasBrushSelection);

    if (_floorHeightPanel)
        _floorHeightPanel->Show(getFloorHeightMode() == 1);

    if (_cornerExtrudePanel)
        _cornerExtrudePanel->Show(getCornerColumns());

    int winMode = getWindowMode();
    if (_windowParamsPanel)
        _windowParamsPanel->Show(winMode != 0);
    if (_windowCountPanel)
        _windowCountPanel->Show(winMode == 2);

    int roof = getRoofType();
    if (_roofHeightPanel)
        _roofHeightPanel->Show(roof == 2 || roof == 3);
    if (_roofBorderPanel)
        _roofBorderPanel->Show(roof == 1);
}

int BuildingGeneratorDialog::getFloorCount()
{
    return findNamedObject<wxSpinCtrl>(_dialog, "BuildingGeneratorFloorCount")->GetValue();
}

int BuildingGeneratorDialog::getFloorHeightMode()
{
    return findNamedObject<wxChoice>(_dialog, "BuildingGeneratorFloorHeightMode")->GetSelection();
}

float BuildingGeneratorDialog::getFloorHeight()
{
    return string::convert<float>(
        findNamedObject<wxTextCtrl>(_dialog, "BuildingGeneratorFloorHeight")->GetValue().ToStdString(), 128.0f);
}

float BuildingGeneratorDialog::getWallThickness()
{
    return string::convert<float>(
        findNamedObject<wxTextCtrl>(_dialog, "BuildingGeneratorWallThickness")->GetValue().ToStdString(), 8.0f);
}

float BuildingGeneratorDialog::getTrimHeight()
{
    return string::convert<float>(
        findNamedObject<wxTextCtrl>(_dialog, "BuildingGeneratorTrimHeight")->GetValue().ToStdString(), 8.0f);
}

int BuildingGeneratorDialog::getWindowMode()
{
    return findNamedObject<wxChoice>(_dialog, "BuildingGeneratorWindowMode")->GetSelection();
}

int BuildingGeneratorDialog::getWindowsPerFloor()
{
    return findNamedObject<wxSpinCtrl>(_dialog, "BuildingGeneratorWindowsPerFloor")->GetValue();
}

float BuildingGeneratorDialog::getWindowWidth()
{
    return string::convert<float>(
        findNamedObject<wxTextCtrl>(_dialog, "BuildingGeneratorWindowWidth")->GetValue().ToStdString(), 48.0f);
}

float BuildingGeneratorDialog::getWindowHeight()
{
    return string::convert<float>(
        findNamedObject<wxTextCtrl>(_dialog, "BuildingGeneratorWindowHeight")->GetValue().ToStdString(), 56.0f);
}

float BuildingGeneratorDialog::getSillHeight()
{
    return string::convert<float>(
        findNamedObject<wxTextCtrl>(_dialog, "BuildingGeneratorSillHeight")->GetValue().ToStdString(), 32.0f);
}

bool BuildingGeneratorDialog::getCornerColumns()
{
    return findNamedObject<wxCheckBox>(_dialog, "BuildingGeneratorCornerColumns")->GetValue();
}

float BuildingGeneratorDialog::getCornerExtrude()
{
    return string::convert<float>(
        findNamedObject<wxTextCtrl>(_dialog, "BuildingGeneratorCornerExtrude")->GetValue().ToStdString(), 0.0f);
}

int BuildingGeneratorDialog::getRoofType()
{
    return findNamedObject<wxChoice>(_dialog, "BuildingGeneratorRoofType")->GetSelection();
}

float BuildingGeneratorDialog::getRoofHeight()
{
    return string::convert<float>(
        findNamedObject<wxTextCtrl>(_dialog, "BuildingGeneratorRoofHeight")->GetValue().ToStdString(), 64.0f);
}

float BuildingGeneratorDialog::getRoofBorderHeight()
{
    return string::convert<float>(
        findNamedObject<wxTextCtrl>(_dialog, "BuildingGeneratorRoofBorderHeight")->GetValue().ToStdString(), 16.0f);
}

float BuildingGeneratorDialog::getBuildingWidth()
{
    return string::convert<float>(
        findNamedObject<wxTextCtrl>(_dialog, "BuildingGeneratorWidth")->GetValue().ToStdString(), 256.0f);
}

float BuildingGeneratorDialog::getBuildingDepth()
{
    return string::convert<float>(
        findNamedObject<wxTextCtrl>(_dialog, "BuildingGeneratorDepth")->GetValue().ToStdString(), 256.0f);
}

float BuildingGeneratorDialog::getBuildingHeight()
{
    return string::convert<float>(
        findNamedObject<wxTextCtrl>(_dialog, "BuildingGeneratorHeight")->GetValue().ToStdString(), 384.0f);
}

std::string BuildingGeneratorDialog::getWallMaterial()
{
    return findNamedObject<wxTextCtrl>(_dialog, "BuildingGeneratorWallMaterial")->GetValue().ToStdString();
}

std::string BuildingGeneratorDialog::getTrimMaterial()
{
    return findNamedObject<wxTextCtrl>(_dialog, "BuildingGeneratorTrimMaterial")->GetValue().ToStdString();
}

void BuildingGeneratorDialog::Show(const cmd::ArgumentList& args)
{
    auto& sel = GlobalSelectionSystem();
    bool hasBrush = false;
    AABB brushBounds;

    if (sel.countSelected() == 1)
    {
        scene::INodePtr node = sel.ultimateSelected();
        if (Node_isBrush(node))
        {
            brushBounds = node->worldAABB();
            if (brushBounds.isValid())
                hasBrush = true;
        }
    }

    double totalHeight = hasBrush
        ? (brushBounds.getExtents().z() * 2.0)
        : 384.0;
    double defaultFloorHeight = totalHeight / 3.0;

    BuildingGeneratorDialog dialog(hasBrush, defaultFloorHeight);

    if (dialog.run() != IDialog::RESULT_OK)
        return;

    building::BuildingParams params;
    params.floorCount = dialog.getFloorCount();
    params.floorHeight = (dialog.getFloorHeightMode() == 1) ? dialog.getFloorHeight() : 0;
    params.wallThickness = dialog.getWallThickness();
    params.trimHeight = dialog.getTrimHeight();

    int winMode = dialog.getWindowMode();
    if (winMode == 0)
        params.windowsPerFloor = -1;
    else if (winMode == 1)
        params.windowsPerFloor = 0;
    else
        params.windowsPerFloor = dialog.getWindowsPerFloor();

    params.cornerColumns = dialog.getCornerColumns();
    params.cornerExtrude = dialog.getCornerExtrude();
    params.windowWidth = dialog.getWindowWidth();
    params.windowHeight = dialog.getWindowHeight();
    params.sillHeight = dialog.getSillHeight();
    params.roofType = dialog.getRoofType();
    params.roofHeight = dialog.getRoofHeight();
    params.roofBorderHeight = dialog.getRoofBorderHeight();
    params.wallMaterial = dialog.getWallMaterial();
    params.trimMaterial = dialog.getTrimMaterial();

    Vector3 mins, maxs;

    if (hasBrush)
    {
        mins = brushBounds.getOrigin() - brushBounds.getExtents();
        maxs = brushBounds.getOrigin() + brushBounds.getExtents();
    }
    else
    {
        Vector3 origin = getSpawnPosition();
        double w = dialog.getBuildingWidth();
        double d = dialog.getBuildingDepth();
        double h = dialog.getBuildingHeight();
        mins = Vector3(origin.x() - w / 2, origin.y() - d / 2, origin.z());
        maxs = Vector3(origin.x() + w / 2, origin.y() + d / 2, origin.z() + h);
    }

    UndoableCommand undo("buildingGeneratorCreate");

    if (hasBrush)
    {
        scene::INodePtr selectedNode = sel.ultimateSelected();
        scene::INodePtr parent = selectedNode->getParent();
        if (parent)
            scene::removeNodeFromParent(selectedNode);
    }

    GlobalSelectionSystem().setSelectedAll(false);

    scene::INodePtr worldspawn = GlobalMapModule().findOrInsertWorldspawn();
    building::generateBuilding(mins, maxs, params, worldspawn);
}

} // namespace ui
