#include "StairsGeneratorDialog.h"
#include "StairsGeometry.h"

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
#include <wx/checkbox.h>
#include <wx/button.h>
#include <wx/statbox.h>

#include "ui/materials/MaterialChooser.h"
#include "ui/materials/MaterialSelector.h"

namespace
{
const char* const WINDOW_TITLE = N_("Stairs Generator");

inline std::string getSelectedShader()
{
    auto selectedShader = GlobalShaderClipboard().getShaderName();
    if (selectedShader.empty())
        selectedShader = texdef_name_default();
    return selectedShader;
}

Vector3 getSpawnPosition()
{
    if (GlobalSelectionSystem().countSelected() > 0)
    {
        AABB bounds = GlobalSelectionSystem().getWorkZone().bounds;
        if (bounds.isValid())
            return bounds.getOrigin();
    }

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

StairsGeneratorDialog::StairsGeneratorDialog()
    : Dialog(_(WINDOW_TITLE), GlobalMainFrame().getWxTopLevelWindow()),
      _spiralPanel(nullptr), _turnPanel(nullptr), _landingPanel(nullptr)
{
    _dialog->GetSizer()->Add(
        loadNamedPanel(_dialog, "StairsGeneratorMainPanel"), 1, wxEXPAND | wxALL, 12);

    wxStaticText* topLabel = findNamedObject<wxStaticText>(_dialog, "StairsGeneratorTopLabel");
    topLabel->SetFont(topLabel->GetFont().Bold());

    // Get panel references for visibility toggling
    _turnPanel = findNamedObject<wxWindow>(_dialog, "StairsGeneratorTurnPanel");
    _landingPanel = findNamedObject<wxWindow>(_dialog, "StairsGeneratorLandingPanel");
    _spiralPanel = findNamedObject<wxWindow>(_dialog, "StairsGeneratorSpiralPanel");

    // Bind events
    findNamedObject<wxChoice>(_dialog, "StairsGeneratorType")
        ->Bind(wxEVT_CHOICE, &StairsGeneratorDialog::onTypeChanged, this);

    findNamedObject<wxButton>(_dialog, "StairsGeneratorBrowseMaterial")
        ->Bind(wxEVT_BUTTON, &StairsGeneratorDialog::onBrowseMaterial, this);

    findNamedObject<wxTextCtrl>(_dialog, "StairsGeneratorMaterial")
        ->SetValue(getSelectedShader());

    updateControlVisibility();
}

void StairsGeneratorDialog::onTypeChanged(wxCommandEvent& ev)
{
    updateControlVisibility();
    _dialog->Layout();
    _dialog->Fit();
}

void StairsGeneratorDialog::onBrowseMaterial(wxCommandEvent& ev)
{
    wxTextCtrl* materialEntry = findNamedObject<wxTextCtrl>(_dialog, "StairsGeneratorMaterial");
    MaterialChooser chooser(_dialog, MaterialSelector::TextureFilter::Regular, materialEntry);
    chooser.ShowModal();
}

void StairsGeneratorDialog::updateControlVisibility()
{
    int type = getType();
    bool showTurn = (type == 1 || type == 2);    // L or U
    bool showLanding = (type == 2);               // U only
    bool showSpiral = (type == 3);                // Spiral

    if (_turnPanel) _turnPanel->Show(showTurn);
    if (_landingPanel) _landingPanel->Show(showLanding);
    if (_spiralPanel) _spiralPanel->Show(showSpiral);
}

int StairsGeneratorDialog::getType()
{
    return findNamedObject<wxChoice>(_dialog, "StairsGeneratorType")->GetSelection();
}

int StairsGeneratorDialog::getStepCount()
{
    return findNamedObject<wxSpinCtrl>(_dialog, "StairsGeneratorStepCount")->GetValue();
}

float StairsGeneratorDialog::getStepHeight()
{
    return string::convert<float>(
        findNamedObject<wxTextCtrl>(_dialog, "StairsGeneratorStepHeight")->GetValue().ToStdString(), 16.0f);
}

float StairsGeneratorDialog::getStepDepth()
{
    return string::convert<float>(
        findNamedObject<wxTextCtrl>(_dialog, "StairsGeneratorStepDepth")->GetValue().ToStdString(), 16.0f);
}

float StairsGeneratorDialog::getWidth()
{
    return string::convert<float>(
        findNamedObject<wxTextCtrl>(_dialog, "StairsGeneratorWidth")->GetValue().ToStdString(), 64.0f);
}

int StairsGeneratorDialog::getDirection()
{
    return findNamedObject<wxChoice>(_dialog, "StairsGeneratorDirection")->GetSelection();
}

bool StairsGeneratorDialog::getSolid()
{
    return findNamedObject<wxCheckBox>(_dialog, "StairsGeneratorSolid")->GetValue();
}

std::string StairsGeneratorDialog::getMaterial()
{
    return findNamedObject<wxTextCtrl>(_dialog, "StairsGeneratorMaterial")->GetValue().ToStdString();
}

float StairsGeneratorDialog::getInnerRadius()
{
    return string::convert<float>(
        findNamedObject<wxTextCtrl>(_dialog, "StairsGeneratorInnerRadius")->GetValue().ToStdString(), 32.0f);
}

float StairsGeneratorDialog::getOuterRadius()
{
    return string::convert<float>(
        findNamedObject<wxTextCtrl>(_dialog, "StairsGeneratorOuterRadius")->GetValue().ToStdString(), 96.0f);
}

float StairsGeneratorDialog::getTotalAngle()
{
    return string::convert<float>(
        findNamedObject<wxTextCtrl>(_dialog, "StairsGeneratorTotalAngle")->GetValue().ToStdString(), 360.0f);
}

int StairsGeneratorDialog::getTurnAt()
{
    return findNamedObject<wxSpinCtrl>(_dialog, "StairsGeneratorTurnAt")->GetValue();
}

int StairsGeneratorDialog::getTurnDirection()
{
    return findNamedObject<wxChoice>(_dialog, "StairsGeneratorTurnDirection")->GetSelection();
}

float StairsGeneratorDialog::getLandingDepth()
{
    return string::convert<float>(
        findNamedObject<wxTextCtrl>(_dialog, "StairsGeneratorLandingDepth")->GetValue().ToStdString(), 32.0f);
}

void StairsGeneratorDialog::Show(const cmd::ArgumentList& args)
{
    StairsGeneratorDialog dialog;

    if (dialog.run() != IDialog::RESULT_OK)
        return;

    int type = dialog.getType();
    int stepCount = dialog.getStepCount();
    float stepH = dialog.getStepHeight();
    float stepD = dialog.getStepDepth();
    float width = dialog.getWidth();
    bool solid = dialog.getSolid();
    std::string material = dialog.getMaterial();

    // Direction: 0=East(0), 1=North(90), 2=West(180), 3=South(270)
    double dirDeg = dialog.getDirection() * 90.0;

    Vector3 spawnPos = getSpawnPosition();

    UndoableCommand undo("stairsGeneratorCreate");
    GlobalSelectionSystem().setSelectedAll(false);

    scene::INodePtr worldspawn = GlobalMapModule().findOrInsertWorldspawn();

    switch (type)
    {
    case 0: // Straight
        stairs::generateStraightStairs(spawnPos, stepCount, stepH, stepD, width, dirDeg, solid, material, worldspawn);
        break;
    case 1: // L-Shape
        stairs::generateLShapeStairs(spawnPos, stepCount, stepH, stepD, width, dirDeg, solid,
            dialog.getTurnAt(), dialog.getTurnDirection(), material, worldspawn);
        break;
    case 2: // U-Shape
        stairs::generateUShapeStairs(spawnPos, stepCount, stepH, stepD, width, dirDeg, solid,
            dialog.getTurnAt(), dialog.getTurnDirection(), dialog.getLandingDepth(),
            material, worldspawn);
        break;
    case 3: // Spiral
        stairs::generateSpiralStairs(spawnPos, stepCount, stepH, dirDeg, solid,
            dialog.getInnerRadius(), dialog.getOuterRadius(), dialog.getTotalAngle(),
            material, worldspawn);
        break;
    }
}

} // namespace ui
