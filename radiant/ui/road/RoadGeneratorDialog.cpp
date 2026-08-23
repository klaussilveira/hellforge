#include "RoadGeneratorDialog.h"
#include "RoadGeometry.h"

#include "i18n.h"
#include "ui/imainframe.h"
#include "imap.h"
#include "iscenegraph.h"
#include "iselection.h"
#include "ishaderclipboard.h"
#include "iundo.h"

#include "command/ExecutionNotPossible.h"
#include "gamelib.h"
#include "scenelib.h"
#include "scene/EntityNode.h"
#include "shaderlib.h"
#include "string/convert.h"
#include "wxutil/Bitmap.h"

#include <wx/button.h>
#include <wx/checkbox.h>
#include <wx/choice.h>
#include <wx/spinctrl.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>

#include "ui/cables/CableGeometry.h"
#include "ui/materials/MaterialChooser.h"
#include "ui/materials/MaterialSelector.h"

namespace
{
const char* const WINDOW_TITLE = N_("Road Generator");
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

} // anonymous namespace

namespace ui
{

RoadGeneratorDialog::RoadGeneratorDialog(const std::vector<std::vector<Vector3>>& curves,
                                         const scene::INodePtr& parent)
    : Dialog(_(WINDOW_TITLE), GlobalMainFrame().getWxTopLevelWindow()),
      _curves(curves), _parent(parent)
{
    _dialog->GetSizer()->Add(
        loadNamedPanel(_dialog, "RoadGeneratorMainPanel"), 1, wxEXPAND | wxALL, 12);

    wxStaticText* topLabel = findNamedObject<wxStaticText>(_dialog, "RoadGeneratorTopLabel");
    topLabel->SetFont(topLabel->GetFont().Bold());

    auto applyTextDefault = [&](const std::string& widget, const std::string& key) {
        auto* ctrl = findNamedObject<wxTextCtrl>(_dialog, widget);
        double xrcDefault = string::convert<double>(ctrl->GetValue().ToStdString(), 0.0);
        double value = game::current::getValue<double>("/generators/road/" + key, xrcDefault);
        ctrl->SetValue(string::to_string(static_cast<int>(value)));
    };
    applyTextDefault("RoadGeneratorLaneWidth", "laneWidth");
    applyTextDefault("RoadGeneratorSidewalkWidth", "sidewalkWidth");
    applyTextDefault("RoadGeneratorSidewalkHeight", "sidewalkHeight");
    applyTextDefault("RoadGeneratorCurbRadius", "curbRadius");
    applyTextDefault("RoadGeneratorCornerRadius", "cornerRadius");

    auto applySpinDefault = [&](const std::string& widget, const std::string& key) {
        auto* ctrl = findNamedObject<wxSpinCtrl>(_dialog, widget);
        double value =
            game::current::getValue<double>("/generators/road/" + key, ctrl->GetValue());
        ctrl->SetValue(static_cast<int>(value));
    };
    applySpinDefault("RoadGeneratorSubdivisions", "subdivisions");
    applySpinDefault("RoadGeneratorLanes", "lanes");

    std::string shader = getSelectedShader();

    for (const std::string& name : { "RoadGeneratorMaterial", "RoadGeneratorSidewalkMaterial",
                                     "RoadGeneratorCurbMaterial" })
    {
        findNamedObject<wxTextCtrl>(_dialog, name)->SetValue(shader);
    }

    for (const std::string& name : { "RoadGeneratorBrowseMaterial",
                                     "RoadGeneratorBrowseSidewalkMaterial",
                                     "RoadGeneratorBrowseCurbMaterial" })
    {
        auto* button = findNamedObject<wxButton>(_dialog, name);
        button->SetBitmap(wxutil::GetLocalBitmap("folder16.png"));
        button->Bind(wxEVT_BUTTON, &RoadGeneratorDialog::onBrowseMaterial, this);
    }

    _regenerateTimer.SetOwner(_dialog);
    _dialog->Bind(wxEVT_TIMER, &RoadGeneratorDialog::onRegenerateTimer, this);

    bindParameterEvents(_dialog, this, &RoadGeneratorDialog::onParameterChanged);

    updateControlSensitivity();
    regenerate();
}

void RoadGeneratorDialog::onParameterChanged(wxCommandEvent& ev)
{
    updateControlSensitivity();
    _regenerateTimer.Start(REGENERATE_DELAY, wxTIMER_ONE_SHOT);
}

void RoadGeneratorDialog::onRegenerateTimer(wxTimerEvent& ev)
{
    regenerate();
}

void RoadGeneratorDialog::onBrowseMaterial(wxCommandEvent& ev)
{
    std::string target = "RoadGeneratorMaterial";

    if (ev.GetId() == findNamedObject<wxButton>(_dialog, "RoadGeneratorBrowseSidewalkMaterial")
                          ->GetId())
    {
        target = "RoadGeneratorSidewalkMaterial";
    }
    else if (ev.GetId() ==
             findNamedObject<wxButton>(_dialog, "RoadGeneratorBrowseCurbMaterial")->GetId())
    {
        target = "RoadGeneratorCurbMaterial";
    }

    wxTextCtrl* materialEntry = findNamedObject<wxTextCtrl>(_dialog, target);
    MaterialChooser chooser(_dialog, MaterialSelector::TextureFilter::Regular, materialEntry);
    chooser.ShowModal();
}

void RoadGeneratorDialog::updateControlSensitivity()
{
    bool sidewalk = findNamedObject<wxCheckBox>(_dialog, "RoadGeneratorSidewalk")->GetValue();
    bool roundCurb = findNamedObject<wxChoice>(_dialog, "RoadGeneratorCurbStyle")
                         ->GetSelection() == road::CURB_ROUND;
    bool roundCorner = findNamedObject<wxChoice>(_dialog, "RoadGeneratorCornerStyle")
                           ->GetSelection() == road::CORNER_ROUND;

    for (const std::string& name : { "RoadGeneratorSidewalkWidth",
                                     "RoadGeneratorSidewalkHeight",
                                     "RoadGeneratorSidewalkMaterial",
                                     "RoadGeneratorCurbMaterial" })
    {
        findNamedObject<wxTextCtrl>(_dialog, name)->Enable(sidewalk);
    }

    findNamedObject<wxChoice>(_dialog, "RoadGeneratorCurbStyle")->Enable(sidewalk);
    findNamedObject<wxTextCtrl>(_dialog, "RoadGeneratorCurbRadius")
        ->Enable(sidewalk && roundCurb);
    findNamedObject<wxTextCtrl>(_dialog, "RoadGeneratorCornerRadius")->Enable(roundCorner);
}

double RoadGeneratorDialog::getNumericValue(const std::string& widgetName, double minimum)
{
    double value = string::convert<double>(
        findNamedObject<wxTextCtrl>(_dialog, widgetName)->GetValue().ToStdString(), minimum);

    return value < minimum ? minimum : value;
}

road::RoadParams RoadGeneratorDialog::collectParams()
{
    road::RoadParams params;

    params.subdivisions =
        findNamedObject<wxSpinCtrl>(_dialog, "RoadGeneratorSubdivisions")->GetValue();
    params.lanes = findNamedObject<wxSpinCtrl>(_dialog, "RoadGeneratorLanes")->GetValue();
    params.laneWidth = getNumericValue("RoadGeneratorLaneWidth", 8);
    params.sidewalk = findNamedObject<wxCheckBox>(_dialog, "RoadGeneratorSidewalk")->GetValue();
    params.sidewalkWidth = getNumericValue("RoadGeneratorSidewalkWidth", 1);
    params.sidewalkHeight = getNumericValue("RoadGeneratorSidewalkHeight", 1);
    params.curbStyle =
        findNamedObject<wxChoice>(_dialog, "RoadGeneratorCurbStyle")->GetSelection();
    params.curbRadius = getNumericValue("RoadGeneratorCurbRadius", 0);
    params.cornerStyle =
        findNamedObject<wxChoice>(_dialog, "RoadGeneratorCornerStyle")->GetSelection();
    params.cornerRadius = getNumericValue("RoadGeneratorCornerRadius", 0);
    params.texScale = road::getTextureScale();

    params.roadMaterial =
        findNamedObject<wxTextCtrl>(_dialog, "RoadGeneratorMaterial")->GetValue().ToStdString();
    params.sidewalkMaterial =
        findNamedObject<wxTextCtrl>(_dialog, "RoadGeneratorSidewalkMaterial")
            ->GetValue()
            .ToStdString();
    params.curbMaterial =
        findNamedObject<wxTextCtrl>(_dialog, "RoadGeneratorCurbMaterial")
            ->GetValue()
            .ToStdString();

    return params;
}

void RoadGeneratorDialog::generateInto()
{
    road::generateRoad(_curves, collectParams(), _parent);
}

void RoadGeneratorDialog::regenerate()
{
    _preview.update(_parent, [this]() { generateInto(); });
}

GeneratorPreview& RoadGeneratorDialog::getPreview()
{
    return _preview;
}

void RoadGeneratorDialog::commitToMap()
{
    _preview.commit(_parent, "roadGenerate", [this]() { generateInto(); });
}

void RoadGeneratorDialog::Show(const cmd::ArgumentList& args)
{
    std::vector<std::vector<Vector3>> curves;

    GlobalSelectionSystem().foreachSelected([&](const scene::INodePtr& node)
    {
        auto entityNode = std::dynamic_pointer_cast<EntityNode>(node);

        if (!entityNode)
        {
            return;
        }

        Entity& entity = entityNode->getEntity();
        std::string curveStr = entity.getKeyValue("curve_CatmullRomSpline");

        if (curveStr.empty())
        {
            curveStr = entity.getKeyValue("curve_Nurbs");
        }

        if (curveStr.empty())
        {
            return;
        }

        auto points = cables::parseCurveString(curveStr);

        if (points.size() >= 3)
        {
            curves.push_back(points);
        }
    });

    if (curves.empty())
    {
        throw cmd::ExecutionNotPossible(
            _("Road Generator: select one or more curve entities first."));
    }

    scene::INodePtr worldspawn = GlobalMapModule().findOrInsertWorldspawn();

    RoadGeneratorDialog dialog(curves, worldspawn);

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
