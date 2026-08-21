#include "CutToolDialog.h"

#include "i18n.h"
#include "ui/imainframe.h"
#include "iscenegraph.h"
#include "iselection.h"
#include "ishaderclipboard.h"
#include "iundo.h"

#include "command/ExecutionNotPossible.h"
#include "gamelib.h"
#include "scenelib.h"
#include "shaderlib.h"
#include "string/convert.h"
#include "string/split.h"

#include <string>
#include <vector>

#include <wx/button.h>
#include <wx/checkbox.h>
#include <wx/choice.h>
#include <wx/spinctrl.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>

#include "ui/materials/MaterialChooser.h"
#include "ui/materials/MaterialSelector.h"

namespace
{
const char* const WINDOW_TITLE = N_("Rule Cut");

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

std::vector<cut::CutSource> collectSources()
{
    std::vector<cut::CutSource> sources;

    GlobalSelectionSystem().foreachSelected([&](const scene::INodePtr& node)
    {
        cut::CutSource source = cut::snapshotBrush(node);

        if (source.faces.empty() || !source.parent || !source.bounds.isValid())
        {
            return;
        }

        sources.push_back(source);
    });

    return sources;
}

} // anonymous namespace

namespace ui
{

CutToolDialog::CutToolDialog(const std::vector<cut::CutSource>& sources)
    : Dialog(_(WINDOW_TITLE), GlobalMainFrame().getWxTopLevelWindow()),
      _sources(sources)
{
    _dialog->GetSizer()->Add(
        loadNamedPanel(_dialog, "CutToolMainPanel"), 1, wxEXPAND | wxALL, 12);

    wxStaticText* topLabel = findNamedObject<wxStaticText>(_dialog, "CutToolTopLabel");
    topLabel->SetFont(topLabel->GetFont().Bold());

    auto applyCutDefault = [&](const std::string& widget, const std::string& key) {
        auto* ctrl = findNamedObject<wxTextCtrl>(_dialog, widget);
        double xrcDefault = string::convert<double>(ctrl->GetValue().ToStdString(), 0.0);
        double value = game::current::getValue<double>("/generators/cut/" + key, xrcDefault);
        ctrl->SetValue(string::to_string(static_cast<int>(value)));
    };
    applyCutDefault("CutToolStep", "step");
    applyCutDefault("CutToolOffset", "offset");

    auto* partsCtrl = findNamedObject<wxSpinCtrl>(_dialog, "CutToolParts");
    partsCtrl->SetValue(game::current::getValue<int>("/generators/cut/parts", partsCtrl->GetValue()));

    findNamedObject<wxTextCtrl>(_dialog, "CutToolMaterial")->SetValue(getSelectedShader());

    findNamedObject<wxButton>(_dialog, "CutToolBrowseMaterial")
        ->Bind(wxEVT_BUTTON, &CutToolDialog::onBrowseMaterial, this);

    _regenerateTimer.SetOwner(_dialog);
    _dialog->Bind(wxEVT_TIMER, &CutToolDialog::onRegenerateTimer, this);

    bindParameterEvents(_dialog, this, &CutToolDialog::onParameterChanged);

    updateControlSensitivity();
    updateCutCount();
    regenerate();
}

CutToolDialog::~CutToolDialog()
{
    cut::clearCutPreview(_sources);
}

void CutToolDialog::onBrowseMaterial(wxCommandEvent& ev)
{
    wxTextCtrl* materialEntry = findNamedObject<wxTextCtrl>(_dialog, "CutToolMaterial");
    MaterialChooser chooser(_dialog, MaterialSelector::TextureFilter::Regular, materialEntry);
    chooser.ShowModal();
}

void CutToolDialog::updateControlSensitivity()
{
    int rule = findNamedObject<wxChoice>(_dialog, "CutToolRule")->GetSelection();

    findNamedObject<wxSpinCtrl>(_dialog, "CutToolParts")->Enable(rule == cut::RULE_EQUAL_PARTS);
    findNamedObject<wxTextCtrl>(_dialog, "CutToolStep")->Enable(rule == cut::RULE_SPACING);
    findNamedObject<wxSpinCtrl>(_dialog, "CutToolSubdivisions")->Enable(rule == cut::RULE_SPACING);
    findNamedObject<wxTextCtrl>(_dialog, "CutToolPattern")->Enable(rule == cut::RULE_PATTERN);

    bool anchored = rule != cut::RULE_EQUAL_PARTS;
    findNamedObject<wxTextCtrl>(_dialog, "CutToolOffset")->Enable(anchored);
    findNamedObject<wxChoice>(_dialog, "CutToolAnchor")->Enable(anchored);

    bool overrideMaterial =
        findNamedObject<wxCheckBox>(_dialog, "CutToolOverrideMaterial")->GetValue();

    findNamedObject<wxTextCtrl>(_dialog, "CutToolMaterial")->Enable(overrideMaterial);
    findNamedObject<wxButton>(_dialog, "CutToolBrowseMaterial")->Enable(overrideMaterial);
}

double CutToolDialog::getNumericValue(const std::string& widgetName, double minimum)
{
    double value = string::convert<double>(
        findNamedObject<wxTextCtrl>(_dialog, widgetName)->GetValue().ToStdString(), minimum);

    return value < minimum ? minimum : value;
}

cut::CutParams CutToolDialog::collectParams()
{
    cut::CutParams params;

    params.axis = findNamedObject<wxChoice>(_dialog, "CutToolAxis")->GetSelection();

    params.rule.type = findNamedObject<wxChoice>(_dialog, "CutToolRule")->GetSelection();
    params.rule.parts = findNamedObject<wxSpinCtrl>(_dialog, "CutToolParts")->GetValue();
    params.rule.step = getNumericValue("CutToolStep", 0);
    params.rule.subdivisions =
        findNamedObject<wxSpinCtrl>(_dialog, "CutToolSubdivisions")->GetValue();
    params.rule.offset = string::convert<double>(
        findNamedObject<wxTextCtrl>(_dialog, "CutToolOffset")->GetValue().ToStdString(), 0.0);
    params.rule.anchor = findNamedObject<wxChoice>(_dialog, "CutToolAnchor")->GetSelection();

    std::vector<std::string> tokens;
    string::split(tokens,
        findNamedObject<wxTextCtrl>(_dialog, "CutToolPattern")->GetValue().ToStdString(), ", \t");

    for (const std::string& token : tokens)
    {
        params.rule.pattern.push_back(string::convert<double>(token, 0.0));
    }

    params.overrideMaterial =
        findNamedObject<wxCheckBox>(_dialog, "CutToolOverrideMaterial")->GetValue();
    params.material =
        findNamedObject<wxTextCtrl>(_dialog, "CutToolMaterial")->GetValue().ToStdString();

    return params;
}

std::size_t CutToolDialog::totalCuts()
{
    return cut::countCuts(_sources, collectParams());
}

void CutToolDialog::updateCutCount()
{
    std::size_t total = totalCuts();

    findNamedObject<wxStaticText>(_dialog, "CutToolCount")
        ->SetLabel(wxString::Format("%lu cuts, %lu pieces",
            static_cast<unsigned long>(total),
            static_cast<unsigned long>(total + _sources.size())));
}

void CutToolDialog::regenerate()
{
    cut::setCutPreview(_sources, collectParams());

    SceneChangeNotify();
    GlobalMainFrame().updateAllWindows();
}

void CutToolDialog::clearPreview()
{
    cut::clearCutPreview(_sources);

    SceneChangeNotify();
    GlobalMainFrame().updateAllWindows();
}

void CutToolDialog::commitToMap()
{
    cut::clearCutPreview(_sources);

    cut::applyCuts(_sources, collectParams());
}

void CutToolDialog::onParameterChanged(wxCommandEvent& ev)
{
    updateControlSensitivity();
    updateCutCount();

    _regenerateTimer.Start(REGENERATE_DELAY, wxTIMER_ONE_SHOT);
}

void CutToolDialog::onRegenerateTimer(wxTimerEvent& ev)
{
    regenerate();
}

void CutToolDialog::RunPreset(const cmd::ArgumentList& args)
{
    cut::CutParams params;
    params.rule.type = cut::RULE_EQUAL_PARTS;
    params.rule.parts = args[0].getInt();
    params.axis = args.size() > 1 ? cut::axisFromString(args[1].getString()) : cut::AXIS_LONGEST;

    std::vector<cut::CutSource> sources = collectSources();

    if (sources.empty())
    {
        throw cmd::ExecutionNotPossible(_("Rule Cut: select one or more brushes first."));
    }

    cut::applyCuts(sources, params);
}

void CutToolDialog::Show(const cmd::ArgumentList& args)
{
    std::vector<cut::CutSource> sources = collectSources();

    if (sources.empty())
    {
        throw cmd::ExecutionNotPossible(_("Rule Cut: select one or more brushes first."));
    }

    CutToolDialog dialog(sources);

    if (dialog.run() == IDialog::RESULT_OK && dialog.totalCuts() > 0)
    {
        dialog.commitToMap();
    }
    else
    {
        dialog.clearPreview();
    }
}

} // namespace ui
