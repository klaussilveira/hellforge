#include "TerrainSculptPanel.h"

#include "i18n.h"
#include "iscenegraph.h"
#include "iselection.h"
#include "ipatch.h"
#include "irender.h"
#include "ui/imainframe.h"
#include "selection/TerrainSculptTool.h"
#include "TerrainSculptPreview.h"

#include <wx/sizer.h>
#include <wx/choice.h>
#include <wx/stattext.h>

namespace ui
{

namespace
{
    class PatchCollector : public scene::NodeVisitor
    {
    public:
        std::vector<scene::INodePtr> patches;

        bool pre(const scene::INodePtr& node) override
        {
            if (Node_isPatch(node)) patches.push_back(node);
            return true;
        }
    };
}

TerrainSculptPanel::TerrainSculptPanel(wxWindow* parent) :
    DockablePanel(parent),
    _targetChoice(nullptr),
    _modeChoice(nullptr),
    _brushChoice(nullptr),
    _falloffTypeChoice(nullptr),
    _radius(nullptr),
    _strength(nullptr),
    _falloff(nullptr),
    _filterRadius(nullptr),
    _flattenHeight(nullptr),
    _noiseAlgorithm(nullptr),
    _noiseScale(nullptr),
    _noiseAmount(nullptr),
    _noiseSeed(nullptr),
    _filterRadiusLabel(nullptr),
    _flattenHeightLabel(nullptr),
    _noiseAlgorithmLabel(nullptr),
    _noiseScaleLabel(nullptr),
    _noiseAmountLabel(nullptr),
    _noiseSeedLabel(nullptr)
{
    populateWindow();
    refreshTargets();
    pullFromSettings();
    updateModeVisibility();

    _mapEventConnection = GlobalMapModule().signal_mapEvent().connect(
        sigc::mem_fun(*this, &TerrainSculptPanel::onMapEvent));

    _selectionChangedConnection = GlobalSelectionSystem().signal_selectionChanged().connect(
        sigc::mem_fun(*this, &TerrainSculptPanel::onSelectionChanged));

    _settingsChangedConnection = TerrainSculptSettings::Instance().signal_settingsChanged.connect(
        sigc::mem_fun(*this, &TerrainSculptPanel::pullFromSettings));

    _preview = std::make_unique<TerrainSculptPreview>();
}

TerrainSculptPanel::~TerrainSculptPanel()
{
    _mapEventConnection.disconnect();
    _selectionChangedConnection.disconnect();
    _settingsChangedConnection.disconnect();

    if (_preview && _previewAttached)
    {
        _preview->clear();
        GlobalRenderSystem().detachRenderable(*_preview);
        _previewAttached = false;
    }
    _preview.reset();

    auto& s = TerrainSculptSettings::Instance();
    s.panelActive = false;
    s.hoverValid = false;
    GlobalMainFrame().updateAllWindows();
}

void TerrainSculptPanel::populateWindow()
{
    auto* main = new wxBoxSizer(wxVERTICAL);

    auto* targetRow = new wxBoxSizer(wxHORIZONTAL);
    targetRow->Add(new wxStaticText(this, wxID_ANY, _("Terrain:")),
                   0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 6);
    _targetChoice = new wxChoice(this, wxID_ANY);
    targetRow->Add(_targetChoice, 1, wxEXPAND);
    main->Add(targetRow, 0, wxEXPAND | wxALL, 8);

    auto* grid = new wxFlexGridSizer(2, 4, 8);
    grid->AddGrowableCol(1, 1);

    grid->Add(new wxStaticText(this, wxID_ANY, _("Tool:")),
              0, wxALIGN_CENTER_VERTICAL);
    _modeChoice = new wxChoice(this, wxID_ANY);
    _modeChoice->Append(_("Raise"));
    _modeChoice->Append(_("Lower"));
    _modeChoice->Append(_("Smooth"));
    _modeChoice->Append(_("Flatten"));
    _modeChoice->Append(_("Noise"));
    grid->Add(_modeChoice, 1, wxEXPAND);

    grid->Add(new wxStaticText(this, wxID_ANY, _("Brush:")),
              0, wxALIGN_CENTER_VERTICAL);
    _brushChoice = new wxChoice(this, wxID_ANY);
    _brushChoice->Append(_("Circle Brush"));
    _brushChoice->SetSelection(0);
    grid->Add(_brushChoice, 1, wxEXPAND);

    grid->Add(new wxStaticText(this, wxID_ANY, _("Radius:")),
              0, wxALIGN_CENTER_VERTICAL);
    _radius = new wxSpinCtrlDouble(this, wxID_ANY);
    _radius->SetRange(1.0, 4096.0);
    _radius->SetIncrement(4.0);
    _radius->SetDigits(2);
    grid->Add(_radius, 1, wxEXPAND);

    grid->Add(new wxStaticText(this, wxID_ANY, _("Strength:")),
              0, wxALIGN_CENTER_VERTICAL);
    _strength = new wxSpinCtrlDouble(this, wxID_ANY);
    _strength->SetRange(0.01, 1024.0);
    _strength->SetIncrement(0.5);
    _strength->SetDigits(2);
    grid->Add(_strength, 1, wxEXPAND);

    grid->Add(new wxStaticText(this, wxID_ANY, _("Falloff:")),
              0, wxALIGN_CENTER_VERTICAL);
    _falloff = new wxSpinCtrlDouble(this, wxID_ANY);
    _falloff->SetRange(0.0, 1.0);
    _falloff->SetIncrement(0.05);
    _falloff->SetDigits(2);
    grid->Add(_falloff, 1, wxEXPAND);

    grid->Add(new wxStaticText(this, wxID_ANY, _("Falloff type:")),
              0, wxALIGN_CENTER_VERTICAL);
    _falloffTypeChoice = new wxChoice(this, wxID_ANY);
    _falloffTypeChoice->Append(_("Smooth"));
    _falloffTypeChoice->Append(_("Linear"));
    _falloffTypeChoice->Append(_("Spherical"));
    _falloffTypeChoice->Append(_("Tip"));
    grid->Add(_falloffTypeChoice, 1, wxEXPAND);

    _filterRadiusLabel = new wxStaticText(this, wxID_ANY, _("Filter radius:"));
    grid->Add(_filterRadiusLabel, 0, wxALIGN_CENTER_VERTICAL);
    _filterRadius = new wxSpinCtrlDouble(this, wxID_ANY);
    _filterRadius->SetRange(0.1, 4.0);
    _filterRadius->SetIncrement(0.1);
    _filterRadius->SetDigits(2);
    grid->Add(_filterRadius, 1, wxEXPAND);

    _flattenHeightLabel = new wxStaticText(this, wxID_ANY, _("Target height:"));
    grid->Add(_flattenHeightLabel, 0, wxALIGN_CENTER_VERTICAL);
    _flattenHeight = new wxSpinCtrlDouble(this, wxID_ANY);
    _flattenHeight->SetRange(-65536.0, 65536.0);
    _flattenHeight->SetIncrement(1.0);
    _flattenHeight->SetDigits(2);
    grid->Add(_flattenHeight, 1, wxEXPAND);

    _noiseAlgorithmLabel = new wxStaticText(this, wxID_ANY, _("Noise algorithm:"));
    grid->Add(_noiseAlgorithmLabel, 0, wxALIGN_CENTER_VERTICAL);
    _noiseAlgorithm = new wxChoice(this, wxID_ANY);
    _noiseAlgorithm->Append(_("Perlin"));
    _noiseAlgorithm->Append(_("Simplex"));
    _noiseAlgorithm->Append(_("fBm"));
    _noiseAlgorithm->Append(_("Ridged Multifractal"));
    grid->Add(_noiseAlgorithm, 1, wxEXPAND);

    _noiseScaleLabel = new wxStaticText(this, wxID_ANY, _("Noise scale:"));
    grid->Add(_noiseScaleLabel, 0, wxALIGN_CENTER_VERTICAL);
    _noiseScale = new wxSpinCtrlDouble(this, wxID_ANY);
    _noiseScale->SetRange(0.001, 1.0);
    _noiseScale->SetIncrement(0.005);
    _noiseScale->SetDigits(4);
    grid->Add(_noiseScale, 1, wxEXPAND);

    _noiseAmountLabel = new wxStaticText(this, wxID_ANY, _("Noise amount:"));
    grid->Add(_noiseAmountLabel, 0, wxALIGN_CENTER_VERTICAL);
    _noiseAmount = new wxSpinCtrlDouble(this, wxID_ANY);
    _noiseAmount->SetRange(0.1, 1024.0);
    _noiseAmount->SetIncrement(1.0);
    _noiseAmount->SetDigits(2);
    grid->Add(_noiseAmount, 1, wxEXPAND);

    _noiseSeedLabel = new wxStaticText(this, wxID_ANY, _("Noise seed:"));
    grid->Add(_noiseSeedLabel, 0, wxALIGN_CENTER_VERTICAL);
    _noiseSeed = new wxSpinCtrl(this, wxID_ANY);
    _noiseSeed->SetRange(0, 2147483647);
    grid->Add(_noiseSeed, 1, wxEXPAND);

    main->Add(grid, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 8);

    auto* shortcuts = new wxFlexGridSizer(2, 4, 8);
    const struct { const wxString keys; const wxString desc; } bindings[] = {
        { "Ctrl+Alt+LMB",           _("Sculpt (drag)") },
        { "Ctrl+Alt+RMB",           _("Inverted sculpt (Raise <-> Lower)") },
        { "[ / ]",                  _("Decrease / increase brush size") },
        { "Shift+[ / Shift+]",      _("Decrease / increase strength") },
    };
    for (const auto& b : bindings)
    {
        auto* keyLbl = new wxStaticText(this, wxID_ANY, b.keys);
        wxFont f = keyLbl->GetFont();
        f.SetWeight(wxFONTWEIGHT_BOLD);
        keyLbl->SetFont(f);
        shortcuts->Add(keyLbl, 0, wxALIGN_CENTER_VERTICAL);
        shortcuts->Add(new wxStaticText(this, wxID_ANY, b.desc), 0, wxALIGN_CENTER_VERTICAL);
    }
    main->Add(shortcuts, 0, wxLEFT | wxRIGHT | wxBOTTOM, 8);

    SetSizerAndFit(main);

    _targetChoice->Bind(wxEVT_CHOICE, &TerrainSculptPanel::onTargetChanged, this);
    _modeChoice->Bind(wxEVT_CHOICE, &TerrainSculptPanel::onChoiceChange, this);
    _falloffTypeChoice->Bind(wxEVT_CHOICE, &TerrainSculptPanel::onChoiceChange, this);
    _radius->Bind(wxEVT_SPINCTRLDOUBLE, &TerrainSculptPanel::onSpinChange, this);
    _strength->Bind(wxEVT_SPINCTRLDOUBLE, &TerrainSculptPanel::onSpinChange, this);
    _falloff->Bind(wxEVT_SPINCTRLDOUBLE, &TerrainSculptPanel::onSpinChange, this);
    _filterRadius->Bind(wxEVT_SPINCTRLDOUBLE, &TerrainSculptPanel::onSpinChange, this);
    _flattenHeight->Bind(wxEVT_SPINCTRLDOUBLE, &TerrainSculptPanel::onSpinChange, this);
    _noiseAlgorithm->Bind(wxEVT_CHOICE, &TerrainSculptPanel::onChoiceChange, this);
    _noiseScale->Bind(wxEVT_SPINCTRLDOUBLE, &TerrainSculptPanel::onSpinChange, this);
    _noiseAmount->Bind(wxEVT_SPINCTRLDOUBLE, &TerrainSculptPanel::onSpinChange, this);
    _noiseSeed->Bind(wxEVT_SPINCTRL, &TerrainSculptPanel::onSeedChange, this);
}

void TerrainSculptPanel::refreshTargets()
{
    _targets.clear();
    _targetChoice->Clear();

    auto root = GlobalSceneGraph().root();
    if (!root) return;

    PatchCollector collector;
    root->traverse(collector);

    scene::INodePtr previous = TerrainSculptSettings::Instance().target.lock();
    int indexToSelect = -1;

    for (const auto& node : collector.patches)
    {
        _targets.push_back(node);
        _targetChoice->Append(node->name());
        if (node == previous)
        {
            indexToSelect = static_cast<int>(_targets.size()) - 1;
        }
    }

    if (indexToSelect >= 0)
    {
        _targetChoice->SetSelection(indexToSelect);
    }
    else if (!_targets.empty())
    {
        _targetChoice->SetSelection(0);
        TerrainSculptSettings::Instance().target = _targets.front();
    }
    else
    {
        TerrainSculptSettings::Instance().target.reset();
    }
}

void TerrainSculptPanel::pullFromSettings()
{
    auto& s = TerrainSculptSettings::Instance();
    _modeChoice->SetSelection(static_cast<int>(s.mode));
    _falloffTypeChoice->SetSelection(static_cast<int>(s.falloffType));
    _radius->SetValue(s.radius);
    _strength->SetValue(s.strength);
    _falloff->SetValue(s.falloff);
    _filterRadius->SetValue(s.smoothFilterRadius);
    _flattenHeight->SetValue(s.flattenHeight);
    _noiseAlgorithm->SetSelection(static_cast<int>(s.noiseAlgorithm));
    _noiseScale->SetValue(s.noiseScale);
    _noiseAmount->SetValue(s.noiseAmount);
    _noiseSeed->SetValue(static_cast<int>(s.noiseSeed));
}

void TerrainSculptPanel::pushToSettings()
{
    auto& s = TerrainSculptSettings::Instance();
    int mode = _modeChoice->GetSelection();
    if (mode >= 0) s.mode = static_cast<TerrainSculptMode>(mode);
    int ft = _falloffTypeChoice->GetSelection();
    if (ft >= 0) s.falloffType = static_cast<TerrainBrushFalloff>(ft);
    s.radius = static_cast<float>(_radius->GetValue());
    s.strength = static_cast<float>(_strength->GetValue());
    s.falloff = static_cast<float>(_falloff->GetValue());
    s.smoothFilterRadius = static_cast<float>(_filterRadius->GetValue());
    s.flattenHeight = static_cast<float>(_flattenHeight->GetValue());
    int alg = _noiseAlgorithm->GetSelection();
    if (alg >= 0) s.noiseAlgorithm = static_cast<noise::Algorithm>(alg);
    s.noiseScale = static_cast<float>(_noiseScale->GetValue());
    s.noiseAmount = static_cast<float>(_noiseAmount->GetValue());
    s.noiseSeed = static_cast<unsigned int>(_noiseSeed->GetValue());
}

void TerrainSculptPanel::updateModeVisibility()
{
    auto mode = TerrainSculptSettings::Instance().mode;
    bool smoothVisible = (mode == TerrainSculptMode::Smooth);
    bool flattenVisible = (mode == TerrainSculptMode::Flatten);
    bool noiseVisible = (mode == TerrainSculptMode::Noise);

    _filterRadiusLabel->Show(smoothVisible);
    _filterRadius->Show(smoothVisible);
    _flattenHeightLabel->Show(flattenVisible);
    _flattenHeight->Show(flattenVisible);
    _noiseAlgorithmLabel->Show(noiseVisible);
    _noiseAlgorithm->Show(noiseVisible);
    _noiseScaleLabel->Show(noiseVisible);
    _noiseScale->Show(noiseVisible);
    _noiseAmountLabel->Show(noiseVisible);
    _noiseAmount->Show(noiseVisible);
    _noiseSeedLabel->Show(noiseVisible);
    _noiseSeed->Show(noiseVisible);

    Layout();
}

void TerrainSculptPanel::onTargetChanged(wxCommandEvent&)
{
    int idx = _targetChoice->GetSelection();
    auto& s = TerrainSculptSettings::Instance();
    scene::INodePtr newTarget;
    if (idx >= 0 && idx < static_cast<int>(_targets.size()))
    {
        newTarget = _targets[idx].lock();
    }
    if (newTarget != s.target.lock())
    {
        s.flattenHeightExplicit = false;
    }
    s.target = newTarget;
}

void TerrainSculptPanel::onChoiceChange(wxCommandEvent&)
{
    pushToSettings();
    updateModeVisibility();
}

void TerrainSculptPanel::onSpinChange(wxSpinDoubleEvent& ev)
{
    if (ev.GetEventObject() == _flattenHeight)
    {
        TerrainSculptSettings::Instance().flattenHeightExplicit = true;
    }
    pushToSettings();
}

void TerrainSculptPanel::onSeedChange(wxSpinEvent&)
{
    pushToSettings();
}

void TerrainSculptPanel::onMapEvent(IMap::MapEvent ev)
{
    if (ev == IMap::MapLoaded || ev == IMap::MapUnloaded)
    {
        refreshTargets();
    }
}

void TerrainSculptPanel::onSelectionChanged(const ISelectable& selectable)
{
    if (!selectable.isSelected()) return;
    if (GlobalSelectionSystem().countSelected() == 0) return;

    scene::INodePtr node = GlobalSelectionSystem().ultimateSelected();
    if (!node || !Node_isPatch(node)) return;

    setTarget(node);
}

void TerrainSculptPanel::setTarget(const scene::INodePtr& node)
{
    auto& s = TerrainSculptSettings::Instance();
    scene::INodePtr current = s.target.lock();
    if (current != node)
    {
        s.flattenHeightExplicit = false;
    }

    for (std::size_t i = 0; i < _targets.size(); ++i)
    {
        if (_targets[i].lock() == node)
        {
            _targetChoice->SetSelection(static_cast<int>(i));
            s.target = node;
            return;
        }
    }

    _targets.push_back(node);
    _targetChoice->Append(node->name());
    _targetChoice->SetSelection(static_cast<int>(_targets.size()) - 1);
    s.target = node;
}

void TerrainSculptPanel::onPanelActivated()
{
    refreshTargets();
    pullFromSettings();
    updateModeVisibility();

    if (_preview && !_previewAttached)
    {
        GlobalRenderSystem().attachRenderable(*_preview);
        _previewAttached = true;
    }
    TerrainSculptSettings::Instance().panelActive = true;
}

void TerrainSculptPanel::onPanelDeactivated()
{
    if (_preview && _previewAttached)
    {
        _preview->clear();
        GlobalRenderSystem().detachRenderable(*_preview);
        _previewAttached = false;
    }

    auto& s = TerrainSculptSettings::Instance();
    s.panelActive = false;
    s.hoverValid = false;
    GlobalMainFrame().updateAllWindows();
}

}
