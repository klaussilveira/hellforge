#include "CableGeneratorDialog.h"

#include "i18n.h"
#include "ui/imainframe.h"
#include "imap.h"
#include "iselection.h"
#include "icurve.h"
#include "ishaderclipboard.h"
#include "iundo.h"
#include "iregistry.h"

#include "string/convert.h"
#include "selectionlib.h"
#include "scenelib.h"
#include "scene/EntityNode.h"
#include "shaderlib.h"
#include "math/Vector3.h"

#include <wx/stattext.h>
#include <wx/textctrl.h>
#include <wx/spinctrl.h>
#include <wx/choice.h>
#include <wx/checkbox.h>
#include <wx/button.h>
#include <wx/bmpbuttn.h>
#include <wx/statbox.h>
#include <wx/msgdlg.h>
#include <wx/textdlg.h>
#include <random>

#include "ui/materials/MaterialChooser.h"
#include "ui/materials/MaterialSelector.h"

namespace
{
const char* const WINDOW_TITLE = N_("Cable Generator");
const char* const RKEY_CABLE_PRESETS = "user/ui/cableGenerator/presets";

inline std::string getSelectedShader()
{
    auto selectedShader = GlobalShaderClipboard().getShaderName();
    if (selectedShader.empty())
        selectedShader = texdef_name_default();
    return selectedShader;
}

enum class InputMode
{
    None,
    Waypoints,
    Curve
};

struct InputData
{
    InputMode mode = InputMode::None;
    std::vector<Vector3> waypoints;
};

InputData detectInput()
{
    InputData data;

    auto& sel = GlobalSelectionSystem();

    // getSelectedFaceCount() returns only faces selected via Ctrl+Shift+Click.
    // foreachFace() visits all faces of primitive-selected brushes first,
    // then component-selected faces last. We collect all and take the last N.
    std::size_t faceCount = sel.getSelectedFaceCount();
    if (faceCount >= 2)
    {
        std::vector<Vector3> allCenters;
        sel.foreachFace([&](IFace& face) {
            const IWinding& winding = face.getWinding();
            Vector3 center(0, 0, 0);
            for (std::size_t i = 0; i < winding.size(); ++i)
                center += winding[i].vertex;
            if (winding.size() > 0)
                center /= static_cast<double>(winding.size());
            allCenters.push_back(center);
        });

        // Component-selected faces are appended last by foreachFace
        if (allCenters.size() >= faceCount)
        {
            data.waypoints.assign(allCenters.end() - faceCount, allCenters.end());
            data.mode = InputMode::Waypoints;
            return data;
        }
        data.waypoints.clear();
    }

    std::size_t selectedCount = sel.countSelected();

    if (selectedCount == 1)
    {
        scene::INodePtr node = sel.ultimateSelected();

        auto entityNode = std::dynamic_pointer_cast<EntityNode>(node);
        if (entityNode)
        {
            Entity& entity = entityNode->getEntity();

            std::string curveStr = entity.getKeyValue("curve_CatmullRomSpline");
            if (curveStr.empty())
                curveStr = entity.getKeyValue("curve_Nurbs");

            if (!curveStr.empty())
            {
                auto points = cables::parseCurveString(curveStr);
                if (points.size() >= 2)
                {
                    data.mode = InputMode::Curve;
                    data.waypoints = points;
                    return data;
                }
            }
        }
    }

    if (selectedCount >= 2)
    {
        sel.foreachSelected([&](const scene::INodePtr& node) {
            AABB bounds = node->worldAABB();
            if (bounds.isValid())
                data.waypoints.push_back(bounds.getOrigin());
        });

        if (data.waypoints.size() >= 2)
        {
            data.mode = InputMode::Waypoints;
            return data;
        }
        data.waypoints.clear();
    }

    return data;
}

cables::CableParams makePreset(int count, float radius, float spacing,
    float spacingVariation, float minScale, float maxScale,
    int circRes, int circResVariation, int lengthRes,
    bool capEnds, int simType, float gravityMin, float gravityMax,
    float gravityMultiplier)
{
    cables::CableParams p;
    p.count = count;
    p.radius = radius;
    p.spacing = spacing;
    p.spacingVariation = spacingVariation;
    p.randomScale = 1.0f;
    p.minScale = minScale;
    p.maxScale = maxScale;
    p.circResolution = circRes;
    p.circResVariation = circResVariation;
    p.lengthResolution = lengthRes;
    p.capEnds = capEnds;
    p.simType = simType;
    p.gravityMin = gravityMin;
    p.gravityMax = gravityMax;
    p.gravityMultiplier = gravityMultiplier;
    return p;
}

void writePresetToRegistry(const std::string& name, const cables::CableParams& p)
{
    auto node = GlobalRegistry().createKeyWithName(RKEY_CABLE_PRESETS, "preset", name);
    node.setAttributeValue("count", string::to_string(p.count));
    node.setAttributeValue("density", string::to_string(p.density));
    node.setAttributeValue("radius", string::to_string(p.radius));
    node.setAttributeValue("spacing", string::to_string(p.spacing));
    node.setAttributeValue("spacingVariation", string::to_string(p.spacingVariation));
    node.setAttributeValue("spacingSeed", string::to_string(p.spacingSeed));
    node.setAttributeValue("randomScale", string::to_string(p.randomScale));
    node.setAttributeValue("minScale", string::to_string(p.minScale));
    node.setAttributeValue("maxScale", string::to_string(p.maxScale));
    node.setAttributeValue("lengthResolution", string::to_string(p.lengthResolution));
    node.setAttributeValue("circResolution", string::to_string(p.circResolution));
    node.setAttributeValue("circResVariation", string::to_string(p.circResVariation));
    node.setAttributeValue("capEnds", string::to_string(p.capEnds ? 1 : 0));
    node.setAttributeValue("material", p.material);
    node.setAttributeValue("fixedSubdivisions", string::to_string(p.fixedSubdivisions ? 1 : 0));
    node.setAttributeValue("subdivisionU", string::to_string(p.subdivisionU));
    node.setAttributeValue("subdivisionV", string::to_string(p.subdivisionV));
    node.setAttributeValue("simType", string::to_string(p.simType));
    node.setAttributeValue("gravityMin", string::to_string(p.gravityMin));
    node.setAttributeValue("gravityMax", string::to_string(p.gravityMax));
    node.setAttributeValue("gravitySeed", string::to_string(p.gravitySeed));
    node.setAttributeValue("gravityMultiplier", string::to_string(p.gravityMultiplier));
}

cables::CableParams readPresetFromNode(const xml::Node& node)
{
    cables::CableParams p;
    auto attr = [&](const std::string& key) { return node.getAttributeValue(key); };

    if (!attr("count").empty()) p.count = string::convert<int>(attr("count"));
    if (!attr("density").empty()) p.density = string::convert<float>(attr("density"));
    if (!attr("radius").empty()) p.radius = string::convert<float>(attr("radius"));
    if (!attr("spacing").empty()) p.spacing = string::convert<float>(attr("spacing"));
    if (!attr("spacingVariation").empty()) p.spacingVariation = string::convert<float>(attr("spacingVariation"));
    if (!attr("spacingSeed").empty()) p.spacingSeed = string::convert<int>(attr("spacingSeed"));
    if (!attr("randomScale").empty()) p.randomScale = string::convert<float>(attr("randomScale"));
    if (!attr("minScale").empty()) p.minScale = string::convert<float>(attr("minScale"));
    if (!attr("maxScale").empty()) p.maxScale = string::convert<float>(attr("maxScale"));
    if (!attr("lengthResolution").empty()) p.lengthResolution = string::convert<int>(attr("lengthResolution"));
    if (!attr("circResolution").empty()) p.circResolution = string::convert<int>(attr("circResolution"));
    if (!attr("circResVariation").empty()) p.circResVariation = string::convert<int>(attr("circResVariation"));
    if (!attr("capEnds").empty()) p.capEnds = string::convert<int>(attr("capEnds")) != 0;
    if (!attr("material").empty()) p.material = attr("material");
    if (!attr("fixedSubdivisions").empty()) p.fixedSubdivisions = string::convert<int>(attr("fixedSubdivisions")) != 0;
    if (!attr("subdivisionU").empty()) p.subdivisionU = string::convert<int>(attr("subdivisionU"));
    if (!attr("subdivisionV").empty()) p.subdivisionV = string::convert<int>(attr("subdivisionV"));
    if (!attr("simType").empty()) p.simType = string::convert<int>(attr("simType"));
    if (!attr("gravityMin").empty()) p.gravityMin = string::convert<float>(attr("gravityMin"));
    if (!attr("gravityMax").empty()) p.gravityMax = string::convert<float>(attr("gravityMax"));
    if (!attr("gravitySeed").empty()) p.gravitySeed = string::convert<int>(attr("gravitySeed"));
    if (!attr("gravityMultiplier").empty()) p.gravityMultiplier = string::convert<float>(attr("gravityMultiplier"));

    return p;
}

} // anonymous namespace

namespace ui
{

CableGeneratorDialog::CableGeneratorDialog()
    : Dialog(_(WINDOW_TITLE), GlobalMainFrame().getWxTopLevelWindow()),
      _gravityPanel(nullptr),
      _presetChoice(nullptr)
{
    _dialog->GetSizer()->Add(
        loadNamedPanel(_dialog, "CableGeneratorMainPanel"), 1, wxEXPAND | wxALL, 12);

    wxStaticText* topLabel = findNamedObject<wxStaticText>(_dialog, "CableGeneratorTopLabel");
    topLabel->SetFont(topLabel->GetFont().Bold());

    _gravityPanel = findNamedObject<wxWindow>(_dialog, "CableGeneratorGravityPanel");
    _presetChoice = findNamedObject<wxChoice>(_dialog, "CableGeneratorPreset");

    _presetChoice->Bind(wxEVT_CHOICE, &CableGeneratorDialog::onPresetSelected, this);

    findNamedObject<wxButton>(_dialog, "CableGeneratorSavePreset")
        ->Bind(wxEVT_BUTTON, &CableGeneratorDialog::onSavePreset, this);

    findNamedObject<wxButton>(_dialog, "CableGeneratorDeletePreset")
        ->Bind(wxEVT_BUTTON, &CableGeneratorDialog::onDeletePreset, this);

    findNamedObject<wxChoice>(_dialog, "CableGeneratorSimType")
        ->Bind(wxEVT_CHOICE, &CableGeneratorDialog::onSimTypeChanged, this);

    findNamedObject<wxCheckBox>(_dialog, "CableGeneratorFixedSubdivisions")
        ->Bind(wxEVT_CHECKBOX, &CableGeneratorDialog::onFixedSubdivisionsChanged, this);

    findNamedObject<wxButton>(_dialog, "CableGeneratorBrowseMaterial")
        ->Bind(wxEVT_BUTTON, &CableGeneratorDialog::onBrowseMaterial, this);

    findNamedObject<wxBitmapButton>(_dialog, "CableGeneratorRandomizeSpacingSeed")
        ->Bind(wxEVT_BUTTON, &CableGeneratorDialog::onRandomizeSeed, this);

    findNamedObject<wxBitmapButton>(_dialog, "CableGeneratorRandomizeGravitySeed")
        ->Bind(wxEVT_BUTTON, &CableGeneratorDialog::onRandomizeSeed, this);

    findNamedObject<wxTextCtrl>(_dialog, "CableGeneratorMaterial")
        ->SetValue(getSelectedShader());

    loadPresets();
    populatePresetChoice();
    updateControlVisibility();
}

void CableGeneratorDialog::loadPresets()
{
    _presets.clear();

    // Built-in presets
    _presets.push_back({"Water Pipes", true,
        makePreset(2, 6, 16, 0, 0.8f, 1.2f, 12, 0, 8, true, 0, 0, 0, 0)});

    _presets.push_back({"Gas Pipes", true,
        makePreset(3, 3, 10, 0, 0.9f, 1.1f, 10, 0, 8, true, 0, 0, 0, 0)});

    _presets.push_back({"Slum Cables", true,
        makePreset(8, 1, 3, 2, 0.5f, 1.5f, 6, 2, 16, false, 1, 0.2f, 0.6f, 1.0f)});

    _presets.push_back({"Sci-Fi Cables", true,
        makePreset(5, 2, 4, 1, 0.5f, 1.5f, 8, 3, 16, true, 0, 0, 0, 0)});

    _presets.push_back({"Power Lines", true,
        makePreset(3, 1.5f, 12, 0, 0.8f, 1.0f, 8, 0, 24, false, 1, 0.1f, 0.15f, 1.0f)});

    // User presets from registry
    auto nodes = GlobalRegistry().findXPath(std::string(RKEY_CABLE_PRESETS) + "/preset");
    for (auto& node : nodes)
    {
        std::string name = node.getAttributeValue("name");
        if (name.empty())
            continue;

        // Skip if it matches a built-in name
        bool isBuiltIn = false;
        for (auto& bp : _presets)
        {
            if (bp.name == name)
            {
                isBuiltIn = true;
                break;
            }
        }
        if (isBuiltIn)
            continue;

        _presets.push_back({name, false, readPresetFromNode(node)});
    }
}

void CableGeneratorDialog::populatePresetChoice()
{
    _presetChoice->Clear();
    _presetChoice->Append("(Custom)");

    for (auto& preset : _presets)
        _presetChoice->Append(preset.name);

    _presetChoice->SetSelection(0);
}

void CableGeneratorDialog::applyPreset(const Preset& preset)
{
    const auto& p = preset.params;

    findNamedObject<wxSpinCtrl>(_dialog, "CableGeneratorCount")->SetValue(p.count);
    findNamedObject<wxTextCtrl>(_dialog, "CableGeneratorDensity")->SetValue(string::to_string(p.density));
    findNamedObject<wxTextCtrl>(_dialog, "CableGeneratorSpacing")->SetValue(string::to_string(p.spacing));
    findNamedObject<wxTextCtrl>(_dialog, "CableGeneratorSpacingVariation")->SetValue(string::to_string(p.spacingVariation));
    findNamedObject<wxSpinCtrl>(_dialog, "CableGeneratorSpacingSeed")->SetValue(p.spacingSeed);
    findNamedObject<wxTextCtrl>(_dialog, "CableGeneratorRadius")->SetValue(string::to_string(p.radius));
    findNamedObject<wxTextCtrl>(_dialog, "CableGeneratorRandomScale")->SetValue(string::to_string(p.randomScale));
    findNamedObject<wxTextCtrl>(_dialog, "CableGeneratorMinScale")->SetValue(string::to_string(p.minScale));
    findNamedObject<wxTextCtrl>(_dialog, "CableGeneratorMaxScale")->SetValue(string::to_string(p.maxScale));
    findNamedObject<wxSpinCtrl>(_dialog, "CableGeneratorLengthRes")->SetValue(p.lengthResolution);
    findNamedObject<wxSpinCtrl>(_dialog, "CableGeneratorCircRes")->SetValue(p.circResolution);
    findNamedObject<wxSpinCtrl>(_dialog, "CableGeneratorCircResVariation")->SetValue(p.circResVariation);
    findNamedObject<wxCheckBox>(_dialog, "CableGeneratorCapEnds")->SetValue(p.capEnds);
    findNamedObject<wxCheckBox>(_dialog, "CableGeneratorFixedSubdivisions")->SetValue(p.fixedSubdivisions);
    findNamedObject<wxSpinCtrl>(_dialog, "CableGeneratorSubdivisionU")->SetValue(p.subdivisionU);
    findNamedObject<wxSpinCtrl>(_dialog, "CableGeneratorSubdivisionV")->SetValue(p.subdivisionV);
    findNamedObject<wxTextCtrl>(_dialog, "CableGeneratorMaterial")->SetValue(p.material);
    findNamedObject<wxChoice>(_dialog, "CableGeneratorSimType")->SetSelection(p.simType);
    findNamedObject<wxTextCtrl>(_dialog, "CableGeneratorGravityMin")->SetValue(string::to_string(p.gravityMin));
    findNamedObject<wxTextCtrl>(_dialog, "CableGeneratorGravityMax")->SetValue(string::to_string(p.gravityMax));
    findNamedObject<wxSpinCtrl>(_dialog, "CableGeneratorGravitySeed")->SetValue(p.gravitySeed);
    findNamedObject<wxTextCtrl>(_dialog, "CableGeneratorGravityMultiplier")->SetValue(string::to_string(p.gravityMultiplier));

    updateControlVisibility();
    _dialog->Layout();
    _dialog->Fit();
}

cables::CableParams CableGeneratorDialog::readParamsFromControls()
{
    cables::CableParams p;
    p.count = getCount();
    p.density = getDensity();
    p.spacing = getSpacing();
    p.spacingVariation = getSpacingVariation();
    p.spacingSeed = getSpacingSeed();
    p.radius = getRadius();
    p.randomScale = getRandomScale();
    p.minScale = getMinScale();
    p.maxScale = getMaxScale();
    p.lengthResolution = getLengthResolution();
    p.circResolution = getCircResolution();
    p.circResVariation = getCircResVariation();
    p.capEnds = getCapEnds();
    p.fixedSubdivisions = getFixedSubdivisions();
    p.subdivisionU = getSubdivisionU();
    p.subdivisionV = getSubdivisionV();
    p.material = getMaterial();
    p.simType = getSimType();
    p.gravityMin = getGravityMin();
    p.gravityMax = getGravityMax();
    p.gravitySeed = getGravitySeed();
    p.gravityMultiplier = getGravityMultiplier();
    return p;
}

void CableGeneratorDialog::onPresetSelected(wxCommandEvent& ev)
{
    int sel = _presetChoice->GetSelection();
    if (sel <= 0 || sel > static_cast<int>(_presets.size()))
        return;

    applyPreset(_presets[sel - 1]);
}

void CableGeneratorDialog::onSavePreset(wxCommandEvent& ev)
{
    wxTextEntryDialog dlg(_dialog, _("Enter preset name:"), _("Save Cable Preset"));

    int sel = _presetChoice->GetSelection();
    if (sel > 0 && sel <= static_cast<int>(_presets.size()) && !_presets[sel - 1].readonly)
        dlg.SetValue(_presets[sel - 1].name);

    if (dlg.ShowModal() != wxID_OK)
        return;

    std::string name = dlg.GetValue().ToStdString();
    if (name.empty())
        return;

    // Reject overwriting built-in presets
    for (auto& p : _presets)
    {
        if (p.name == name && p.readonly)
        {
            wxMessageBox(_("Cannot overwrite a built-in preset."),
                         _(WINDOW_TITLE), wxOK | wxICON_WARNING);
            return;
        }
    }

    auto params = readParamsFromControls();

    // Delete existing registry entry with this name, then write new one
    auto existing = GlobalRegistry().findXPath(
        std::string(RKEY_CABLE_PRESETS) + "/preset[@name='" + name + "']");
    for (auto& node : existing)
        node.erase();

    writePresetToRegistry(name, params);

    // Update or add to _presets
    bool found = false;
    for (auto& p : _presets)
    {
        if (p.name == name)
        {
            p.params = params;
            found = true;
            break;
        }
    }
    if (!found)
        _presets.push_back({name, false, params});

    populatePresetChoice();

    // Select the saved preset
    for (int i = 0; i < static_cast<int>(_presets.size()); ++i)
    {
        if (_presets[i].name == name)
        {
            _presetChoice->SetSelection(i + 1);
            break;
        }
    }
}

void CableGeneratorDialog::onDeletePreset(wxCommandEvent& ev)
{
    int sel = _presetChoice->GetSelection();
    if (sel <= 0 || sel > static_cast<int>(_presets.size()))
        return;

    auto& preset = _presets[sel - 1];
    if (preset.readonly)
    {
        wxMessageBox(_("Cannot delete a built-in preset."),
                     _(WINDOW_TITLE), wxOK | wxICON_WARNING);
        return;
    }

    auto nodes = GlobalRegistry().findXPath(
        std::string(RKEY_CABLE_PRESETS) + "/preset[@name='" + preset.name + "']");
    for (auto& node : nodes)
        node.erase();

    _presets.erase(_presets.begin() + (sel - 1));
    populatePresetChoice();
}

void CableGeneratorDialog::onSimTypeChanged(wxCommandEvent& ev)
{
    updateControlVisibility();
    _dialog->Layout();
    _dialog->Fit();
}

void CableGeneratorDialog::onFixedSubdivisionsChanged(wxCommandEvent& ev)
{
    updateControlVisibility();
    _dialog->Layout();
    _dialog->Fit();
}

void CableGeneratorDialog::onRandomizeSeed(wxCommandEvent& ev)
{
    wxWindow* btn = static_cast<wxWindow*>(ev.GetEventObject());
    wxString name = btn->GetName();

    std::random_device rd;
    int seed = std::uniform_int_distribution<int>(0, 99999)(rd);

    if (name == "CableGeneratorRandomizeSpacingSeed")
        findNamedObject<wxSpinCtrl>(_dialog, "CableGeneratorSpacingSeed")->SetValue(seed);
    else if (name == "CableGeneratorRandomizeGravitySeed")
        findNamedObject<wxSpinCtrl>(_dialog, "CableGeneratorGravitySeed")->SetValue(seed);
}

void CableGeneratorDialog::onBrowseMaterial(wxCommandEvent& ev)
{
    wxTextCtrl* materialEntry = findNamedObject<wxTextCtrl>(_dialog, "CableGeneratorMaterial");
    MaterialChooser chooser(_dialog, MaterialSelector::TextureFilter::Regular, materialEntry);
    chooser.ShowModal();
}

void CableGeneratorDialog::updateControlVisibility()
{
    bool showGravity = (getSimType() == 1);
    if (_gravityPanel) _gravityPanel->Show(showGravity);

    bool showSubdiv = getFixedSubdivisions();
    if (auto* panel = findNamedObject<wxWindow>(_dialog, "CableGeneratorSubdivisionPanel"))
        panel->Show(showSubdiv);
}

int CableGeneratorDialog::getCount()
{
    return findNamedObject<wxSpinCtrl>(_dialog, "CableGeneratorCount")->GetValue();
}

float CableGeneratorDialog::getDensity()
{
    return string::convert<float>(
        findNamedObject<wxTextCtrl>(_dialog, "CableGeneratorDensity")->GetValue().ToStdString(), 1.0f);
}

float CableGeneratorDialog::getSpacing()
{
    return string::convert<float>(
        findNamedObject<wxTextCtrl>(_dialog, "CableGeneratorSpacing")->GetValue().ToStdString(), 4.0f);
}

float CableGeneratorDialog::getSpacingVariation()
{
    return string::convert<float>(
        findNamedObject<wxTextCtrl>(_dialog, "CableGeneratorSpacingVariation")->GetValue().ToStdString(), 0.0f);
}

int CableGeneratorDialog::getSpacingSeed()
{
    return findNamedObject<wxSpinCtrl>(_dialog, "CableGeneratorSpacingSeed")->GetValue();
}

float CableGeneratorDialog::getRadius()
{
    return string::convert<float>(
        findNamedObject<wxTextCtrl>(_dialog, "CableGeneratorRadius")->GetValue().ToStdString(), 2.0f);
}

float CableGeneratorDialog::getRandomScale()
{
    return string::convert<float>(
        findNamedObject<wxTextCtrl>(_dialog, "CableGeneratorRandomScale")->GetValue().ToStdString(), 0.0f);
}

float CableGeneratorDialog::getMinScale()
{
    return string::convert<float>(
        findNamedObject<wxTextCtrl>(_dialog, "CableGeneratorMinScale")->GetValue().ToStdString(), 0.8f);
}

float CableGeneratorDialog::getMaxScale()
{
    return string::convert<float>(
        findNamedObject<wxTextCtrl>(_dialog, "CableGeneratorMaxScale")->GetValue().ToStdString(), 1.2f);
}

int CableGeneratorDialog::getLengthResolution()
{
    return findNamedObject<wxSpinCtrl>(_dialog, "CableGeneratorLengthRes")->GetValue();
}

int CableGeneratorDialog::getCircResolution()
{
    return findNamedObject<wxSpinCtrl>(_dialog, "CableGeneratorCircRes")->GetValue();
}

int CableGeneratorDialog::getCircResVariation()
{
    return findNamedObject<wxSpinCtrl>(_dialog, "CableGeneratorCircResVariation")->GetValue();
}

bool CableGeneratorDialog::getCapEnds()
{
    return findNamedObject<wxCheckBox>(_dialog, "CableGeneratorCapEnds")->GetValue();
}

bool CableGeneratorDialog::getFixedSubdivisions()
{
    return findNamedObject<wxCheckBox>(_dialog, "CableGeneratorFixedSubdivisions")->GetValue();
}

int CableGeneratorDialog::getSubdivisionU()
{
    return findNamedObject<wxSpinCtrl>(_dialog, "CableGeneratorSubdivisionU")->GetValue();
}

int CableGeneratorDialog::getSubdivisionV()
{
    return findNamedObject<wxSpinCtrl>(_dialog, "CableGeneratorSubdivisionV")->GetValue();
}

std::string CableGeneratorDialog::getMaterial()
{
    return findNamedObject<wxTextCtrl>(_dialog, "CableGeneratorMaterial")->GetValue().ToStdString();
}

int CableGeneratorDialog::getSimType()
{
    return findNamedObject<wxChoice>(_dialog, "CableGeneratorSimType")->GetSelection();
}

float CableGeneratorDialog::getGravityMin()
{
    return string::convert<float>(
        findNamedObject<wxTextCtrl>(_dialog, "CableGeneratorGravityMin")->GetValue().ToStdString(), 0.1f);
}

float CableGeneratorDialog::getGravityMax()
{
    return string::convert<float>(
        findNamedObject<wxTextCtrl>(_dialog, "CableGeneratorGravityMax")->GetValue().ToStdString(), 0.5f);
}

int CableGeneratorDialog::getGravitySeed()
{
    return findNamedObject<wxSpinCtrl>(_dialog, "CableGeneratorGravitySeed")->GetValue();
}

float CableGeneratorDialog::getGravityMultiplier()
{
    return string::convert<float>(
        findNamedObject<wxTextCtrl>(_dialog, "CableGeneratorGravityMultiplier")->GetValue().ToStdString(), 1.0f);
}

void CableGeneratorDialog::Show(const cmd::ArgumentList& args)
{
    InputData input = detectInput();

    if (input.mode == InputMode::None)
    {
        wxMessageBox(_("Select 2+ faces (Ctrl+Shift+Click), 2+ brushes/entities, or a curve entity."),
                     _(WINDOW_TITLE), wxOK | wxICON_WARNING);
        return;
    }

    CableGeneratorDialog dialog;

    if (dialog.run() != IDialog::RESULT_OK)
        return;

    cables::CableParams params = dialog.readParamsFromControls();

    UndoableCommand undo("cableGeneratorCreate");
    GlobalSelectionSystem().setSelectedAll(false);
    GlobalSelectionSystem().setSelectedAllComponents(false);

    scene::INodePtr worldspawn = GlobalMapModule().findOrInsertWorldspawn();

    if (input.mode == InputMode::Curve)
    {
        cables::generateCablesAlongPath(input.waypoints, params, worldspawn);
    }
    else
    {
        for (size_t i = 0; i + 1 < input.waypoints.size(); ++i)
            cables::generateCablesBetweenPoints(input.waypoints[i], input.waypoints[i + 1], params, worldspawn);
    }
}

} // namespace ui
