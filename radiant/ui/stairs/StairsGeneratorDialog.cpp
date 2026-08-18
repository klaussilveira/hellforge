#include "StairsGeneratorDialog.h"
#include "StairsGeometry.h"

#include "i18n.h"
#include "ui/imainframe.h"
#include "ui/common/GeneratorSpawn.h"
#include "imap.h"
#include "iscenegraph.h"
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


} // anonymous namespace

namespace ui
{

StairsGeneratorDialog::StairsGeneratorDialog(const scene::INodePtr& parent)
    : Dialog(_(WINDOW_TITLE), GlobalMainFrame().getWxTopLevelWindow()),
      _spiralPanel(nullptr), _turnPanel(nullptr), _landingPanel(nullptr), _parent(parent)
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

    auto applyStairsDefault = [&](const std::string& widget, const std::string& key) {
        auto* ctrl = findNamedObject<wxTextCtrl>(_dialog, widget);
        double xrcDefault = string::convert<double>(ctrl->GetValue().ToStdString(), 0.0);
        double v = game::current::getValue<double>("/generators/stairs/" + key, xrcDefault);
        ctrl->SetValue(string::to_string(static_cast<int>(v)));
    };
    applyStairsDefault("StairsGeneratorStepHeight", "stepHeight");
    applyStairsDefault("StairsGeneratorStepDepth", "stepDepth");
    applyStairsDefault("StairsGeneratorWidth", "width");
    applyStairsDefault("StairsGeneratorLandingDepth", "landingDepth");
    applyStairsDefault("StairsGeneratorInnerRadius", "innerRadius");
    applyStairsDefault("StairsGeneratorOuterRadius", "outerRadius");

    auto* stepCountCtrl = findNamedObject<wxSpinCtrl>(_dialog, "StairsGeneratorStepCount");
    stepCountCtrl->SetValue(
        game::current::getValue<int>("/generators/stairs/stepCount", stepCountCtrl->GetValue()));

    updateControlVisibility();

    bindParameterEvents(_dialog, this, &StairsGeneratorDialog::onParameterChanged);

    regenerate();
}

GeneratorPreview& StairsGeneratorDialog::getPreview()
{
    return _preview;
}

void StairsGeneratorDialog::onParameterChanged(wxCommandEvent& ev)
{
    regenerate();
}

void StairsGeneratorDialog::generateInto()
{
    int stepCount = getStepCount();
    float stepH = getStepHeight();
    float stepD = getStepDepth();
    float width = getWidth();

    if (stepCount < 1 || stepH <= 0 || stepD <= 0 || width <= 0)
    {
        return;
    }

    int type = getType();
    bool solid = getSolid();
    std::string material = getMaterial();
    double dirDeg = getDirection() * 90.0;

    double reach = std::max(256.0, static_cast<double>(stepCount) * stepD);
    Vector3 spawnPos = getGeneratorSpawnPosition(reach);

    switch (type)
    {
    case 0:
        stairs::generateStraightStairs(spawnPos, stepCount, stepH, stepD, width, dirDeg,
            solid, material, _parent);
        break;
    case 1:
        stairs::generateLShapeStairs(spawnPos, stepCount, stepH, stepD, width, dirDeg, solid,
            getTurnAt(), getTurnDirection(), material, _parent);
        break;
    case 2:
        stairs::generateUShapeStairs(spawnPos, stepCount, stepH, stepD, width, dirDeg, solid,
            getTurnAt(), getTurnDirection(), getLandingDepth(), material, _parent);
        break;
    case 3:
        stairs::generateSpiralStairs(spawnPos, stepCount, stepH, dirDeg, solid,
            getInnerRadius(), getOuterRadius(), getTotalAngle(), material, _parent);
        break;
    }
}

void StairsGeneratorDialog::regenerate()
{
    _preview.update(_parent, [this]() { generateInto(); });
}

void StairsGeneratorDialog::commitToMap()
{
    _preview.commit(_parent, "stairsGeneratorCreate", [this]() { generateInto(); });
}

void StairsGeneratorDialog::onTypeChanged(wxCommandEvent& ev)
{
    updateControlVisibility();
    _dialog->Layout();
    _dialog->Fit();
    regenerate();
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
    scene::INodePtr worldspawn = GlobalMapModule().findOrInsertWorldspawn();

    StairsGeneratorDialog dialog(worldspawn);

    if (dialog.run() == IDialog::RESULT_OK)
    {
        dialog.commitToMap();
    }
    else
    {
        dialog.getPreview().clear();
    }
}

} // namespace ui
