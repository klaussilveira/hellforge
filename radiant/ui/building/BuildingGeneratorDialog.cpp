#include "BuildingGeneratorDialog.h"
#include "BuildingGeometry.h"

#include "i18n.h"
#include "ui/imainframe.h"
#include "ui/common/GeneratorSpawn.h"
#include "imap.h"
#include "iscenegraph.h"
#include "iselection.h"
#include "ishaderclipboard.h"
#include "iundo.h"

#include "gamelib.h"
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


template<typename T>
T gameDefault(const std::string& key, T fallback)
{
    return game::current::getValue<T>("/generators/building/" + key, fallback);
}

} // anonymous namespace

namespace ui
{

BuildingGeneratorDialog::BuildingGeneratorDialog(bool hasBrushSelection, double defaultFloorHeight,
                                                 const AABB& brushBounds,
                                                 const scene::INodePtr& parent)
    : Dialog(_(WINDOW_TITLE), GlobalMainFrame().getWxTopLevelWindow()),
      _dimensionsPanel(nullptr),
      _floorHeightPanel(nullptr),
      _windowParamsPanel(nullptr),
      _windowCountPanel(nullptr),
      _cornerExtrudePanel(nullptr),
      _roofHeightPanel(nullptr),
      _roofBorderPanel(nullptr),
      _hasBrushSelection(hasBrushSelection),
      _brushBounds(brushBounds),
      _parent(parent)
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

    auto setText = [&](const std::string& widget, const std::string& key) {
        auto* ctrl = findNamedObject<wxTextCtrl>(_dialog, widget);
        double xrcDefault = string::convert<double>(ctrl->GetValue().ToStdString(), 0.0);
        ctrl->SetValue(string::to_string(static_cast<int>(gameDefault<double>(key, xrcDefault))));
    };

    if (!_hasBrushSelection)
    {
        setText("BuildingGeneratorWidth", "width");
        setText("BuildingGeneratorDepth", "depth");
        setText("BuildingGeneratorHeight", "height");
    }

    setText("BuildingGeneratorWallThickness", "wallThickness");
    setText("BuildingGeneratorTrimHeight", "trimHeight");
    setText("BuildingGeneratorWindowWidth", "windowWidth");
    setText("BuildingGeneratorWindowHeight", "windowHeight");
    setText("BuildingGeneratorSillHeight", "sillHeight");
    setText("BuildingGeneratorCornerExtrude", "cornerExtrude");
    setText("BuildingGeneratorRoofHeight", "roofHeight");
    setText("BuildingGeneratorRoofBorderHeight", "roofBorderHeight");

    auto* floorCountCtrl = findNamedObject<wxSpinCtrl>(_dialog, "BuildingGeneratorFloorCount");
    floorCountCtrl->SetValue(gameDefault<int>("floorCount", floorCountCtrl->GetValue()));

    int fh = static_cast<int>(defaultFloorHeight);
    findNamedObject<wxTextCtrl>(_dialog, "BuildingGeneratorFloorHeight")
        ->SetValue(string::to_string(fh));

    updateControlVisibility();

    bindParameterEvents(_dialog, this, &BuildingGeneratorDialog::onParameterChanged);

    regenerate();
}

GeneratorPreview& BuildingGeneratorDialog::getPreview()
{
    return _preview;
}

void BuildingGeneratorDialog::onParameterChanged(wxCommandEvent& ev)
{
    regenerate();
}

void BuildingGeneratorDialog::generateInto()
{
    building::BuildingParams params;
    params.floorCount = getFloorCount();
    params.floorHeight = (getFloorHeightMode() == 1) ? getFloorHeight() : 0;
    params.wallThickness = getWallThickness();
    params.trimHeight = getTrimHeight();

    int winMode = getWindowMode();
    if (winMode == 0)
        params.windowsPerFloor = -1;
    else if (winMode == 1)
        params.windowsPerFloor = 0;
    else
        params.windowsPerFloor = getWindowsPerFloor();

    params.cornerColumns = getCornerColumns();
    params.cornerExtrude = getCornerExtrude();
    params.windowWidth = getWindowWidth();
    params.windowHeight = getWindowHeight();
    params.sillHeight = getSillHeight();
    params.roofType = getRoofType();
    params.roofHeight = getRoofHeight();
    params.roofBorderHeight = getRoofBorderHeight();
    params.doorWidth = gameDefault<double>("doorWidth", params.doorWidth);
    params.doorHeight = gameDefault<double>("doorHeight", params.doorHeight);
    params.wallMaterial = getWallMaterial();
    params.trimMaterial = getTrimMaterial();

    Vector3 mins, maxs;

    if (_hasBrushSelection)
    {
        mins = _brushBounds.getOrigin() - _brushBounds.getExtents();
        maxs = _brushBounds.getOrigin() + _brushBounds.getExtents();
    }
    else
    {
        double w = getBuildingWidth();
        double d = getBuildingDepth();
        double h = getBuildingHeight();

        Vector3 spawnPos = getGeneratorSpawnPosition(std::max(256.0, std::max(w, d)));

        mins = Vector3(spawnPos.x() - w / 2, spawnPos.y() - d / 2, spawnPos.z());
        maxs = Vector3(spawnPos.x() + w / 2, spawnPos.y() + d / 2, spawnPos.z() + h);
    }

    if (params.floorCount < 1 || maxs.x() - mins.x() < 1 ||
        maxs.y() - mins.y() < 1 || maxs.z() - mins.z() < 1)
    {
        return;
    }

    building::generateBuilding(mins, maxs, params, _parent);
}

void BuildingGeneratorDialog::regenerate()
{
    _preview.update(_parent, [this]() { generateInto(); });
}

void BuildingGeneratorDialog::commitToMap()
{
    _preview.commit(_parent, "buildingGeneratorCreate", [this]() { generateInto(); });
}

void BuildingGeneratorDialog::onFloorHeightModeChanged(wxCommandEvent& ev)
{
    updateControlVisibility();
    _dialog->Layout();
    _dialog->Fit();
    regenerate();
}

void BuildingGeneratorDialog::onWindowModeChanged(wxCommandEvent& ev)
{
    updateControlVisibility();
    _dialog->Layout();
    _dialog->Fit();
    regenerate();
}

void BuildingGeneratorDialog::onRoofTypeChanged(wxCommandEvent& ev)
{
    updateControlVisibility();
    _dialog->Layout();
    _dialog->Fit();
    regenerate();
}

void BuildingGeneratorDialog::onCornerColumnsChanged(wxCommandEvent& ev)
{
    updateControlVisibility();
    _dialog->Layout();
    _dialog->Fit();
    regenerate();
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
    scene::INodePtr sourceNode;
    scene::INodePtr sourceParent;

    if (sel.countSelected() == 1)
    {
        scene::INodePtr node = sel.ultimateSelected();
        if (Node_isBrush(node))
        {
            brushBounds = node->worldAABB();
            if (brushBounds.isValid())
            {
                hasBrush = true;
                sourceNode = node;
                sourceParent = node->getParent();
            }
        }
    }

    double cfgHeight = gameDefault<double>("height", 384.0);
    int cfgFloors = std::max(1, gameDefault<int>("floorCount", 3));
    double totalHeight = hasBrush
        ? (brushBounds.getExtents().z() * 2.0)
        : cfgHeight;
    double defaultFloorHeight = hasBrush
        ? (totalHeight / cfgFloors)
        : gameDefault<double>("floorHeight", totalHeight / cfgFloors);

    scene::INodePtr worldspawn = GlobalMapModule().findOrInsertWorldspawn();

    if (sourceNode && sourceParent)
    {
        scene::removeNodeFromParent(sourceNode);
    }

    BuildingGeneratorDialog dialog(hasBrush, defaultFloorHeight, brushBounds, worldspawn);

    auto result = dialog.run();

    dialog.getPreview().clear();

    if (sourceNode && sourceParent)
    {
        scene::addNodeToContainer(sourceNode, sourceParent);
    }

    if (result == IDialog::RESULT_OK)
    {
        UndoableCommand undo("buildingGeneratorCreate");

        if (sourceNode && sourceParent)
        {
            scene::removeNodeFromParent(sourceNode);
        }

        dialog.commitToMap();
    }
    else if (sourceNode)
    {
        Node_setSelected(sourceNode, true);
        SceneChangeNotify();
    }
}

} // namespace ui
