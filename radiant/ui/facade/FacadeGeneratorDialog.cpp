#include "FacadeGeneratorDialog.h"

#include "i18n.h"
#include "ui/imainframe.h"
#include "icameraview.h"
#include "imap.h"
#include "ientity.h"
#include "ipatch.h"
#include "iscenegraph.h"
#include "iselection.h"
#include "ishaderclipboard.h"
#include "iundo.h"

#include "command/ExecutionNotPossible.h"
#include "gamelib.h"
#include "scenelib.h"
#include "selectionlib.h"
#include "shaderlib.h"
#include "string/convert.h"

#include <algorithm>
#include <utility>
#include <vector>

#include <wx/button.h>
#include <wx/checkbox.h>
#include <wx/choice.h>
#include <wx/spinctrl.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>

#include "ui/materials/MaterialChooser.h"
#include "ui/materials/MaterialSelector.h"
#include "wxutil/PickerButton.h"

namespace
{
const char* const WINDOW_TITLE = N_("Facade Generator");

const int REGENERATE_DELAY = 150;

inline std::string getSelectedShader()
{
    auto selectedShader = GlobalShaderClipboard().getShaderName();

    if (selectedShader.empty())
    {
        selectedShader = texdef_name_default();
    }

    return selectedShader;
}

inline std::string getHiddenShader()
{
    return "textures/common/caulk";
}

template<typename T>
T gameDefault(const std::string& key, T fallback)
{
    return game::current::getValue<T>("/generators/facade/" + key, fallback);
}

inline int autoFrontSide()
{
    try
    {
        const Vector3& forward = GlobalCameraManager().getActiveView().getForwardVector();

        if (std::abs(forward.x()) > std::abs(forward.y()))
        {
            return forward.x() > 0 ? facade::FRONT_XNEG : facade::FRONT_XPOS;
        }

        return forward.y() > 0 ? facade::FRONT_YNEG : facade::FRONT_YPOS;
    }
    catch (const std::runtime_error&)
    {
    }

    return facade::FRONT_YNEG;
}

const double TILE_SIZES[] = {0.0, 20.0, 40.0, 80.0, 160.0};

} // anonymous namespace

namespace ui
{
FacadeGeneratorDialog::FacadeGeneratorDialog(bool fromPatch, const AABB& bounds,
                                             const facade::FacadePath& patchPath,
                                             const scene::INodePtr& parent)
    : Dialog(_(WINDOW_TITLE), GlobalMainFrame().getWxTopLevelWindow()),
      _fromPatch(fromPatch),
      _bounds(bounds),
      _patchPath(patchPath),
      _parent(parent),
      _updating(false)
{
    _dialog->GetSizer()->Add(
        loadNamedPanel(_dialog, "FacadeGeneratorMainPanel"), 1, wxEXPAND | wxALL, 12);

    wxStaticText* topLabel = findNamedObject<wxStaticText>(_dialog, "FacadeGeneratorTopLabel");
    topLabel->SetFont(topLabel->GetFont().Bold());

    std::string shader = getSelectedShader();
    findNamedObject<wxTextCtrl>(_dialog, "FacadeGeneratorWallMaterial")->SetValue(shader);
    findNamedObject<wxTextCtrl>(_dialog, "FacadeGeneratorTrimMaterial")->SetValue(shader);
    findNamedObject<wxTextCtrl>(_dialog, "FacadeGeneratorRevealMaterial")->SetValue(shader);
    findNamedObject<wxTextCtrl>(_dialog, "FacadeGeneratorHiddenMaterial")
        ->SetValue(getHiddenShader());

    const std::pair<std::string, std::string> materialSlots[] = {
        {"FacadeGeneratorBrowseWallMaterial", "FacadeGeneratorWallMaterial"},
        {"FacadeGeneratorBrowseTrimMaterial", "FacadeGeneratorTrimMaterial"},
        {"FacadeGeneratorBrowseRevealMaterial", "FacadeGeneratorRevealMaterial"},
        {"FacadeGeneratorBrowseHiddenMaterial", "FacadeGeneratorHiddenMaterial"},
    };

    for (const auto& slot : materialSlots)
    {
        std::string entryName = slot.second;

        auto* browseButton = wxutil::ReplaceWithPickerButton(
            findNamedObject<wxButton>(_dialog, slot.first));
        browseButton->Bind(wxEVT_BUTTON, [this, entryName](wxCommandEvent&)
        {
            wxTextCtrl* entry = findNamedObject<wxTextCtrl>(_dialog, entryName);
            MaterialChooser chooser(_dialog, MaterialSelector::TextureFilter::Regular, entry);
            chooser.ShowModal();
        });
    }

    findNamedObject<wxChoice>(_dialog, "FacadeGeneratorFront")->SetSelection(autoFrontSide());

    applyPreset(facade::PRESET_HONGKONG);

    findNamedObject<wxChoice>(_dialog, "FacadeGeneratorPreset")
        ->Bind(wxEVT_CHOICE, &FacadeGeneratorDialog::onPresetChanged, this);

    _regenerateTimer.SetOwner(_dialog);
    _dialog->Bind(wxEVT_TIMER, &FacadeGeneratorDialog::onRegenerateTimer, this);

    bindParameterEvents(_dialog, this, &FacadeGeneratorDialog::onParameterChanged);

    updateControlSensitivity();
    regenerate();
}

GeneratorPreview& FacadeGeneratorDialog::getPreview()
{
    return _preview;
}

double FacadeGeneratorDialog::getNumericValue(const std::string& widgetName, double minimum)
{
    double value = string::convert<double>(
        findNamedObject<wxTextCtrl>(_dialog, widgetName)->GetValue().ToStdString(), minimum);

    return value < minimum ? minimum : value;
}

void FacadeGeneratorDialog::setNumericValue(const std::string& widgetName, double value)
{
    findNamedObject<wxTextCtrl>(_dialog, widgetName)
        ->SetValue(string::to_string(static_cast<int>(value)));
}

void FacadeGeneratorDialog::applyPreset(int preset)
{
    facade::FacadeParams params = facade::getPreset(preset);

    _updating = true;

    setNumericValue("FacadeGeneratorWallThickness",
                    gameDefault<double>("wallThickness", params.wallThickness));

    setNumericValue("FacadeGeneratorGroundHeight", params.ground.height);
    setNumericValue("FacadeGeneratorGroundCount", params.ground.bayCount);
    setNumericValue("FacadeGeneratorGroundPitch", params.ground.bayPitch);
    setNumericValue("FacadeGeneratorGroundOpeningWidth", params.ground.openingWidth);
    setNumericValue("FacadeGeneratorGroundOpeningHeight", params.ground.openingHeight);
    setNumericValue("FacadeGeneratorGroundSill", params.ground.sillHeight);
    setNumericValue("FacadeGeneratorDoorWidth", gameDefault<double>("doorWidth", params.doorWidth));
    setNumericValue("FacadeGeneratorDoorHeight",
                    gameDefault<double>("doorHeight", params.doorHeight));

    setNumericValue("FacadeGeneratorUpperHeight", params.upper.height);
    setNumericValue("FacadeGeneratorUpperOffset", params.upper.frontOffset);
    setNumericValue("FacadeGeneratorUpperCount", params.upper.bayCount);
    setNumericValue("FacadeGeneratorUpperPitch", params.upper.bayPitch);
    setNumericValue("FacadeGeneratorUpperOpeningWidth", params.upper.openingWidth);
    setNumericValue("FacadeGeneratorUpperOpeningHeight", params.upper.openingHeight);
    setNumericValue("FacadeGeneratorUpperSill", params.upper.sillHeight);

    findNamedObject<wxCheckBox>(_dialog, "FacadeGeneratorTopFloor")
        ->SetValue(params.hasTopFloor);
    setNumericValue("FacadeGeneratorTopHeight", params.top.height);
    setNumericValue("FacadeGeneratorTopCount", params.top.bayCount);
    setNumericValue("FacadeGeneratorTopPitch", params.top.bayPitch);
    setNumericValue("FacadeGeneratorTopOpeningWidth", params.top.openingWidth);
    setNumericValue("FacadeGeneratorTopOpeningHeight", params.top.openingHeight);
    setNumericValue("FacadeGeneratorTopSill", params.top.sillHeight);

    setNumericValue("FacadeGeneratorPlinthHeight", params.plinthHeight);
    setNumericValue("FacadeGeneratorPlinthProud", params.plinthProud);
    setNumericValue("FacadeGeneratorPierProud", params.pierProud);
    setNumericValue("FacadeGeneratorTrimProud", params.trimProud);
    setNumericValue("FacadeGeneratorCourseHeight", params.courseHeight);
    setNumericValue("FacadeGeneratorCourseProud", params.courseProud);
    setNumericValue("FacadeGeneratorCorniceHeight", params.corniceHeight);
    setNumericValue("FacadeGeneratorCorniceProud", params.corniceProud);
    setNumericValue("FacadeGeneratorParapetHeight", params.parapetHeight);

    findNamedObject<wxCheckBox>(_dialog, "FacadeGeneratorDoor")->SetValue(params.groundDoor);
    findNamedObject<wxCheckBox>(_dialog, "FacadeGeneratorColumns")->SetValue(params.arcadeColumns);

    findNamedObject<wxSpinCtrl>(_dialog, "FacadeGeneratorFloorCount")
        ->SetValue(gameDefault<int>("floorCount", params.floorCount));

    _updating = false;
}

facade::FacadeParams FacadeGeneratorDialog::collectParams()
{
    facade::FacadeParams params =
        facade::getPreset(findNamedObject<wxChoice>(_dialog, "FacadeGeneratorPreset")->GetSelection());

    params.fitToSource =
        findNamedObject<wxChoice>(_dialog, "FacadeGeneratorHeightMode")->GetSelection() == 0;
    params.floorCount = findNamedObject<wxSpinCtrl>(_dialog, "FacadeGeneratorFloorCount")->GetValue();

    params.wallThickness = getNumericValue("FacadeGeneratorWallThickness", 1);

    params.ground.height = getNumericValue("FacadeGeneratorGroundHeight", facade::GRID);
    params.ground.frontOffset = 0;
    params.ground.bayCount = static_cast<int>(getNumericValue("FacadeGeneratorGroundCount", 0));
    params.ground.bayPitch = getNumericValue("FacadeGeneratorGroundPitch", 0);
    params.ground.openingWidth = getNumericValue("FacadeGeneratorGroundOpeningWidth", 0);
    params.ground.openingHeight = getNumericValue("FacadeGeneratorGroundOpeningHeight", 0);
    params.ground.sillHeight = getNumericValue("FacadeGeneratorGroundSill", 0);

    params.upper.height = getNumericValue("FacadeGeneratorUpperHeight", facade::GRID);
    params.upper.frontOffset = getNumericValue("FacadeGeneratorUpperOffset", 0);
    params.upper.bayCount = static_cast<int>(getNumericValue("FacadeGeneratorUpperCount", 0));
    params.upper.bayPitch = getNumericValue("FacadeGeneratorUpperPitch", 0);
    params.upper.openingWidth = getNumericValue("FacadeGeneratorUpperOpeningWidth", 0);
    params.upper.openingHeight = getNumericValue("FacadeGeneratorUpperOpeningHeight", 0);
    params.upper.sillHeight = getNumericValue("FacadeGeneratorUpperSill", 0);

    params.hasTopFloor =
        findNamedObject<wxCheckBox>(_dialog, "FacadeGeneratorTopFloor")->GetValue();
    params.top.height = getNumericValue("FacadeGeneratorTopHeight", facade::GRID);
    params.top.bayCount = static_cast<int>(getNumericValue("FacadeGeneratorTopCount", 0));
    params.top.bayPitch = getNumericValue("FacadeGeneratorTopPitch", 0);
    params.top.openingWidth = getNumericValue("FacadeGeneratorTopOpeningWidth", 0);
    params.top.openingHeight = getNumericValue("FacadeGeneratorTopOpeningHeight", 0);
    params.top.sillHeight = getNumericValue("FacadeGeneratorTopSill", 0);

    params.groundDoor = findNamedObject<wxCheckBox>(_dialog, "FacadeGeneratorDoor")->GetValue();
    params.doorWidth = getNumericValue("FacadeGeneratorDoorWidth", 0);
    params.doorHeight = getNumericValue("FacadeGeneratorDoorHeight", 0);
    params.arcadeColumns =
        findNamedObject<wxCheckBox>(_dialog, "FacadeGeneratorColumns")->GetValue();

    params.plinthHeight = getNumericValue("FacadeGeneratorPlinthHeight", 0);
    params.plinthProud = getNumericValue("FacadeGeneratorPlinthProud", 0);
    params.pierProud = getNumericValue("FacadeGeneratorPierProud", 0);
    params.trimProud = getNumericValue("FacadeGeneratorTrimProud", 0);
    params.courseHeight = getNumericValue("FacadeGeneratorCourseHeight", 0);
    params.courseProud = getNumericValue("FacadeGeneratorCourseProud", 0);
    params.corniceHeight = getNumericValue("FacadeGeneratorCorniceHeight", 0);
    params.corniceProud = getNumericValue("FacadeGeneratorCorniceProud", 0);
    params.parapetHeight = getNumericValue("FacadeGeneratorParapetHeight", 0);

    params.solidBody =
        findNamedObject<wxCheckBox>(_dialog, "FacadeGeneratorSolidBody")->GetValue();
    params.trimAsEntity =
        findNamedObject<wxCheckBox>(_dialog, "FacadeGeneratorTrimEntity")->GetValue();

    int tile = findNamedObject<wxChoice>(_dialog, "FacadeGeneratorTile")->GetSelection();
    params.tileUnits = (tile >= 0 && tile < 5) ? TILE_SIZES[tile] : 0.0;

    params.wallMaterial =
        findNamedObject<wxTextCtrl>(_dialog, "FacadeGeneratorWallMaterial")->GetValue().ToStdString();
    params.trimMaterial =
        findNamedObject<wxTextCtrl>(_dialog, "FacadeGeneratorTrimMaterial")->GetValue().ToStdString();
    params.revealMaterial =
        findNamedObject<wxTextCtrl>(_dialog, "FacadeGeneratorRevealMaterial")->GetValue().ToStdString();
    params.hiddenMaterial =
        findNamedObject<wxTextCtrl>(_dialog, "FacadeGeneratorHiddenMaterial")->GetValue().ToStdString();

    return params;
}

facade::FacadePath FacadeGeneratorDialog::collectPath(double wallThickness)
{
    facade::FacadePath path;

    if (_fromPatch)
    {
        path = _patchPath;
    }
    else
    {
        int front = findNamedObject<wxChoice>(_dialog, "FacadeGeneratorFront")->GetSelection();

        if (front == facade::FRONT_AUTO)
        {
            front = autoFrontSide();
        }

        path = facade::pathFromBounds(_bounds, front, wallThickness);
    }

    if (findNamedObject<wxCheckBox>(_dialog, "FacadeGeneratorFlip")->GetValue())
    {
        std::reverse(path.points.begin(), path.points.end());
    }

    return path;
}

void FacadeGeneratorDialog::generateInto()
{
    facade::FacadeParams params = collectParams();
    facade::FacadePath path = collectPath(params.wallThickness);

    facade::generateFacade(path, params, _parent);
}

void FacadeGeneratorDialog::regenerate()
{
    _preview.update(GlobalSceneGraph().root(), [this]() { generateInto(); });
}

void FacadeGeneratorDialog::commitToMap()
{
    _preview.commit(GlobalSceneGraph().root(), "facadeGenerate",
                    [this]() { generateInto(); });

    std::vector<scene::INodePtr> childPrimitives;

    GlobalSelectionSystem().foreachSelected([&](const scene::INodePtr& node)
    {
        scene::INodePtr nodeParent = node->getParent();

        if (nodeParent && nodeParent != _parent && Node_isEntity(nodeParent))
        {
            childPrimitives.push_back(node);
        }
    });

    for (const scene::INodePtr& node : childPrimitives)
    {
        Node_setSelected(node, false);
    }
}

void FacadeGeneratorDialog::onParameterChanged(wxCommandEvent& ev)
{
    if (_updating)
    {
        return;
    }

    updateControlSensitivity();
    _regenerateTimer.Start(REGENERATE_DELAY, wxTIMER_ONE_SHOT);
}

void FacadeGeneratorDialog::onPresetChanged(wxCommandEvent& ev)
{
    applyPreset(findNamedObject<wxChoice>(_dialog, "FacadeGeneratorPreset")->GetSelection());

    updateControlSensitivity();
    _regenerateTimer.Start(REGENERATE_DELAY, wxTIMER_ONE_SHOT);
}

void FacadeGeneratorDialog::onRegenerateTimer(wxTimerEvent& ev)
{
    regenerate();
}

void FacadeGeneratorDialog::updateControlSensitivity()
{
    findNamedObject<wxChoice>(_dialog, "FacadeGeneratorFront")->Enable(!_fromPatch);
    findNamedObject<wxCheckBox>(_dialog, "FacadeGeneratorSolidBody")->Enable(!_fromPatch);

    bool explicitFloors =
        findNamedObject<wxChoice>(_dialog, "FacadeGeneratorHeightMode")->GetSelection() == 1;

    findNamedObject<wxSpinCtrl>(_dialog, "FacadeGeneratorFloorCount")->Enable(explicitFloors);

    bool door = findNamedObject<wxCheckBox>(_dialog, "FacadeGeneratorDoor")->GetValue();
    findNamedObject<wxTextCtrl>(_dialog, "FacadeGeneratorDoorWidth")->Enable(door);
    findNamedObject<wxTextCtrl>(_dialog, "FacadeGeneratorDoorHeight")->Enable(door);

    const std::pair<std::string, std::string> bayFields[] = {
        {"FacadeGeneratorGroundCount", "FacadeGeneratorGroundPitch"},
        {"FacadeGeneratorUpperCount", "FacadeGeneratorUpperPitch"},
        {"FacadeGeneratorTopCount", "FacadeGeneratorTopPitch"},
    };

    for (const auto& field : bayFields)
    {
        bool automatic = getNumericValue(field.first, 0) < 1;
        findNamedObject<wxTextCtrl>(_dialog, field.second)->Enable(automatic);
    }

    bool topFloor = findNamedObject<wxCheckBox>(_dialog, "FacadeGeneratorTopFloor")->GetValue();

    for (const std::string& name : {"FacadeGeneratorTopHeight", "FacadeGeneratorTopCount",
                                    "FacadeGeneratorTopOpeningWidth",
                                    "FacadeGeneratorTopOpeningHeight",
                                    "FacadeGeneratorTopSill"})
    {
        findNamedObject<wxTextCtrl>(_dialog, name)->Enable(topFloor);
    }

    findNamedObject<wxTextCtrl>(_dialog, "FacadeGeneratorTopPitch")
        ->Enable(topFloor && getNumericValue("FacadeGeneratorTopCount", 0) < 1);
}

void FacadeGeneratorDialog::Show(const cmd::ArgumentList& args)
{
    auto& selection = GlobalSelectionSystem();

    if (selection.countSelected() != 1)
    {
        throw cmd::ExecutionNotPossible(
            _("Facade Generator: select exactly one brush or patch first."));
    }

    scene::INodePtr source = selection.ultimateSelected();
    scene::INodePtr sourceParent = source->getParent();

    bool fromPatch = false;
    AABB bounds;
    facade::FacadePath patchPath;

    if (IPatch* patch = Node_getIPatch(source))
    {
        if (!facade::pathFromPatch(*patch, patchPath))
        {
            throw cmd::ExecutionNotPossible(
                _("Facade Generator: the selected patch is degenerate."));
        }

        fromPatch = true;
    }
    else if (Node_isBrush(source))
    {
        bounds = source->worldAABB();

        if (!bounds.isValid())
        {
            throw cmd::ExecutionNotPossible(
                _("Facade Generator: the selected brush is degenerate."));
        }
    }
    else
    {
        throw cmd::ExecutionNotPossible(
            _("Facade Generator: select exactly one brush or patch first."));
    }

    scene::INodePtr worldspawn = GlobalMapModule().findOrInsertWorldspawn();

    scene::removeNodeFromParent(source);

    FacadeGeneratorDialog dialog(fromPatch, bounds, patchPath, worldspawn);

    auto result = dialog.run();

    dialog.getPreview().clear();

    scene::addNodeToContainer(source, sourceParent);

    if (result == IDialog::RESULT_OK)
    {
        UndoableCommand undo("facadeGenerate");

        scene::removeNodeFromParent(source);

        dialog.commitToMap();
    }
    else
    {
        Node_setSelected(source, true);
        SceneChangeNotify();
    }
}

} // namespace ui
