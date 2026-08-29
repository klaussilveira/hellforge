#include "RoofGeneratorDialog.h"
#include "RoofGeometry.h"

#include "i18n.h"
#include "ui/imainframe.h"
#include "ibrush.h"
#include "imap.h"
#include "iscenegraph.h"
#include "iselection.h"
#include "ishaderclipboard.h"
#include "iundo.h"

#include "command/ExecutionNotPossible.h"
#include "gamelib.h"
#include "scenelib.h"
#include "shaderlib.h"
#include "string/convert.h"

#include <wx/button.h>
#include <wx/choice.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>

#include "ui/materials/MaterialChooser.h"
#include "ui/materials/MaterialSelector.h"
#include "wxutil/PickerButton.h"

namespace
{
const char* const WINDOW_TITLE = N_("Roof Generator");

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

RoofGeneratorDialog::RoofGeneratorDialog(const std::vector<AABB>& walls, const AABB& footprint,
                                         const scene::INodePtr& parent)
    : Dialog(_(WINDOW_TITLE), GlobalMainFrame().getWxTopLevelWindow()),
      _walls(walls), _footprint(footprint), _parent(parent)
{
    _dialog->GetSizer()->Add(
        loadNamedPanel(_dialog, "RoofGeneratorMainPanel"), 1, wxEXPAND | wxALL, 12);

    wxStaticText* topLabel = findNamedObject<wxStaticText>(_dialog, "RoofGeneratorTopLabel");
    topLabel->SetFont(topLabel->GetFont().Bold());

    auto applyRoofDefault = [&](const std::string& widget, const std::string& key) {
        auto* ctrl = findNamedObject<wxTextCtrl>(_dialog, widget);
        double xrcDefault = string::convert<double>(ctrl->GetValue().ToStdString(), 0.0);
        double value = game::current::getValue<double>("/generators/roof/" + key, xrcDefault);
        ctrl->SetValue(string::to_string(static_cast<int>(value)));
    };
    applyRoofDefault("RoofGeneratorHeight", "height");
    applyRoofDefault("RoofGeneratorSlab", "slabThickness");
    applyRoofDefault("RoofGeneratorEave", "eave");
    applyRoofDefault("RoofGeneratorRake", "rake");

    findNamedObject<wxTextCtrl>(_dialog, "RoofGeneratorMaterial")->SetValue(getSelectedShader());

    auto* browseMaterial = wxutil::ReplaceWithPickerButton(
        findNamedObject<wxButton>(_dialog, "RoofGeneratorBrowseMaterial"));
    browseMaterial->Bind(wxEVT_BUTTON, &RoofGeneratorDialog::onBrowseMaterial, this);

    for (const std::string& name : {"RoofGeneratorType", "RoofGeneratorRidge"})
    {
        findNamedObject<wxChoice>(_dialog, name)
            ->Bind(wxEVT_CHOICE, &RoofGeneratorDialog::onParameterChanged, this);
    }

    for (const std::string& name : {"RoofGeneratorHeight", "RoofGeneratorSlab",
                                    "RoofGeneratorEave", "RoofGeneratorRake",
                                    "RoofGeneratorMaterial"})
    {
        findNamedObject<wxTextCtrl>(_dialog, name)
            ->Bind(wxEVT_TEXT, &RoofGeneratorDialog::onParameterChanged, this);
    }

    updateControlSensitivity();
    regenerate();
}

void RoofGeneratorDialog::onParameterChanged(wxCommandEvent& ev)
{
    updateControlSensitivity();
    regenerate();
}

void RoofGeneratorDialog::onBrowseMaterial(wxCommandEvent& ev)
{
    wxTextCtrl* materialEntry = findNamedObject<wxTextCtrl>(_dialog, "RoofGeneratorMaterial");
    MaterialChooser chooser(_dialog, MaterialSelector::TextureFilter::Regular, materialEntry);
    chooser.ShowModal();
}

void RoofGeneratorDialog::updateControlSensitivity()
{
    bool isGable = findNamedObject<wxChoice>(_dialog, "RoofGeneratorType")->GetSelection() ==
                   roof::ROOF_GABLE;

    findNamedObject<wxTextCtrl>(_dialog, "RoofGeneratorRake")->Enable(isGable);
}

double RoofGeneratorDialog::getNumericValue(const std::string& widgetName, double minimum)
{
    double value = string::convert<double>(
        findNamedObject<wxTextCtrl>(_dialog, widgetName)->GetValue().ToStdString(), minimum);

    return value < minimum ? minimum : value;
}

void RoofGeneratorDialog::generateInto()
{
    roof::RoofParams params;
    params.type = findNamedObject<wxChoice>(_dialog, "RoofGeneratorType")->GetSelection();
    params.ridgeAxis = findNamedObject<wxChoice>(_dialog, "RoofGeneratorRidge")->GetSelection();
    params.height = getNumericValue("RoofGeneratorHeight", 1);
    params.slabThickness = getNumericValue("RoofGeneratorSlab", 1);
    params.eave = getNumericValue("RoofGeneratorEave", 0);
    params.rake = getNumericValue("RoofGeneratorRake", 0);
    params.material =
        findNamedObject<wxTextCtrl>(_dialog, "RoofGeneratorMaterial")->GetValue().ToStdString();

    roof::generateRoof(_walls, _footprint, params, _parent);
}

void RoofGeneratorDialog::regenerate()
{
    _preview.update(_parent, [this]() { generateInto(); });
}

GeneratorPreview& RoofGeneratorDialog::getPreview()
{
    return _preview;
}

void RoofGeneratorDialog::commitToMap()
{
    _preview.commit(_parent, "roofGenerate", [this]() { generateInto(); });
}

void RoofGeneratorDialog::Show(const cmd::ArgumentList& args)
{
    std::vector<AABB> selected;
    scene::INodePtr parent;

    GlobalSelectionSystem().foreachSelected([&](const scene::INodePtr& node)
    {
        if (Node_getIBrush(node) == nullptr) return;

        AABB bounds = node->worldAABB();

        if (!bounds.isValid()) return;

        if (!parent)
        {
            parent = node->getParent();
        }

        selected.push_back(bounds);
    });

    if (selected.empty())
    {
        throw cmd::ExecutionNotPossible(_("Roof Generator: select one or more brushes first."));
    }

    auto walls = roof::selectWalls(selected);
    auto footprint = roof::footprintOf(walls);

    if (!parent)
    {
        parent = GlobalMapModule().findOrInsertWorldspawn();
    }

    RoofGeneratorDialog dialog(walls, footprint, parent);

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
