#include "VertexPaintPanel.h"

#include "i18n.h"
#include "ifilesystem.h"
#include "imodel.h"
#include "irender.h"
#include "iscenegraph.h"
#include "ivertexpaintable.h"
#include "ui/imainframe.h"
#include "selection/VertexPaintTool.h"
#include "VertexPaintPreview.h"
#include "math/Matrix4.h"
#include "os/path.h"
#include "os/fs.h"
#include "wxutil/dialog/MessageBox.h"

#include <wx/button.h>
#include <wx/choice.h>
#include <wx/sizer.h>
#include <wx/stattext.h>

namespace ui
{

namespace
{
    std::string resolveAbsolutePath(const std::string& modelPath)
    {
        if (modelPath.empty()) return {};
        if (path_is_absolute(modelPath.c_str())) return modelPath;
        std::string root = GlobalFileSystem().findFile(modelPath);
        if (root.empty()) return {};
        return root + modelPath;
    }

    class PreviewModeWalker : public scene::NodeVisitor
    {
    public:
        model::PaintablePreviewMode mode;

        bool pre(const scene::INodePtr& node) override
        {
            if (auto* paintable = Node_getVertexPaintable(node))
            {
                paintable->setPreviewMode(mode);
            }
            return true;
        }
    };

    class FillWalker : public scene::NodeVisitor
    {
    public:
        std::string modelPath;
        Vector3 colour;

        bool pre(const scene::INodePtr& node) override
        {
            auto* paintable = Node_getVertexPaintable(node);
            if (!paintable) return true;
            if (paintable->getPaintableModelPath() != modelPath) return true;

            bool touched = false;
            std::size_t count = paintable->getPaintableSurfaceCount();
            for (std::size_t i = 0; i < count; ++i)
            {
                if (auto* surf = paintable->getPaintableSurface(i))
                {
                    if (vertexPaint::fillAll(*surf, colour))
                    {
                        touched = true;
                    }
                }
            }
            if (touched)
            {
                paintable->queueRenderableUpdate();
            }
            return true;
        }
    };

    void applyPreviewModeToScene(model::PaintablePreviewMode mode)
    {
        auto root = GlobalSceneGraph().root();
        if (!root) return;

        PreviewModeWalker walker;
        walker.mode = mode;
        root->traverse(walker);
    }
}

VertexPaintPanel::VertexPaintPanel(wxWindow* parent) :
    DockablePanel(parent),
    _channelChoice(nullptr),
    _previewModeChoice(nullptr),
    _falloffTypeChoice(nullptr),
    _radius(nullptr),
    _strength(nullptr),
    _falloff(nullptr),
    _fillRButton(nullptr),
    _fillGButton(nullptr),
    _fillBButton(nullptr),
    _clearButton(nullptr),
    _saveButton(nullptr)
{
    populateWindow();
    pullFromSettings();

    _settingsChangedConnection = VertexPaintSettings::Instance().signal_settingsChanged.connect(
        sigc::mem_fun(*this, &VertexPaintPanel::pullFromSettings));

    _preview = std::make_unique<VertexPaintPreview>();
}

VertexPaintPanel::~VertexPaintPanel()
{
    _settingsChangedConnection.disconnect();

    if (_preview && _previewAttached)
    {
        _preview->clear();
        GlobalRenderSystem().detachRenderable(*_preview);
        _previewAttached = false;
    }
    _preview.reset();

    applyPreviewModeToScene(model::PaintablePreviewMode::Material);

    auto& s = VertexPaintSettings::Instance();
    s.panelActive = false;
    s.hoverValid = false;
    s.target.reset();
    GlobalMainFrame().updateAllWindows();
}

void VertexPaintPanel::populateWindow()
{
    auto* main = new wxBoxSizer(wxVERTICAL);

    auto* grid = new wxFlexGridSizer(2, 4, 8);
    grid->AddGrowableCol(1, 1);

    grid->Add(new wxStaticText(this, wxID_ANY, _("Channel:")),
              0, wxALIGN_CENTER_VERTICAL);
    _channelChoice = new wxChoice(this, wxID_ANY);
    _channelChoice->Append(_("Red"));
    _channelChoice->Append(_("Green"));
    _channelChoice->Append(_("Blue"));
    grid->Add(_channelChoice, 1, wxEXPAND);

    grid->Add(new wxStaticText(this, wxID_ANY, _("View:")),
              0, wxALIGN_CENTER_VERTICAL);
    _previewModeChoice = new wxChoice(this, wxID_ANY);
    _previewModeChoice->Append(_("Material"));
    _previewModeChoice->Append(_("Vertex colours"));
    grid->Add(_previewModeChoice, 1, wxEXPAND);

    grid->Add(new wxStaticText(this, wxID_ANY, _("Radius:")),
              0, wxALIGN_CENTER_VERTICAL);
    _radius = new wxSpinCtrlDouble(this, wxID_ANY);
    _radius->SetRange(0.5, 4096.0);
    _radius->SetIncrement(2.0);
    _radius->SetDigits(2);
    grid->Add(_radius, 1, wxEXPAND);

    grid->Add(new wxStaticText(this, wxID_ANY, _("Strength:")),
              0, wxALIGN_CENTER_VERTICAL);
    _strength = new wxSpinCtrlDouble(this, wxID_ANY);
    _strength->SetRange(0.01, 1.0);
    _strength->SetIncrement(0.05);
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

    main->Add(grid, 0, wxEXPAND | wxALL, 8);

    auto* fillRow = new wxBoxSizer(wxHORIZONTAL);
    _fillRButton = new wxButton(this, wxID_ANY, _("Fill R"));
    _fillGButton = new wxButton(this, wxID_ANY, _("Fill G"));
    _fillBButton = new wxButton(this, wxID_ANY, _("Fill B"));
    _clearButton = new wxButton(this, wxID_ANY, _("Clear all"));
    fillRow->Add(_fillRButton, 1, wxEXPAND | wxRIGHT, 4);
    fillRow->Add(_fillGButton, 1, wxEXPAND | wxRIGHT, 4);
    fillRow->Add(_fillBButton, 1, wxEXPAND | wxRIGHT, 4);
    fillRow->Add(_clearButton, 1, wxEXPAND);
    main->Add(fillRow, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 8);

    _saveButton = new wxButton(this, wxID_ANY, _("Save to .ase"));
    main->Add(_saveButton, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 8);

    auto* shortcuts = new wxFlexGridSizer(2, 4, 8);
    const struct { const wxString keys; const wxString desc; } bindings[] = {
        { "Ctrl+Alt+LMB",           _("Paint channel (drag)") },
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

    _channelChoice->Bind(wxEVT_CHOICE, &VertexPaintPanel::onChoiceChange, this);
    _previewModeChoice->Bind(wxEVT_CHOICE, &VertexPaintPanel::onPreviewModeChanged, this);
    _falloffTypeChoice->Bind(wxEVT_CHOICE, &VertexPaintPanel::onChoiceChange, this);
    _radius->Bind(wxEVT_SPINCTRLDOUBLE, &VertexPaintPanel::onSpinChange, this);
    _strength->Bind(wxEVT_SPINCTRLDOUBLE, &VertexPaintPanel::onSpinChange, this);
    _falloff->Bind(wxEVT_SPINCTRLDOUBLE, &VertexPaintPanel::onSpinChange, this);
    _saveButton->Bind(wxEVT_BUTTON, &VertexPaintPanel::onSaveClicked, this);
    _fillRButton->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) {
        fillTargetWithColour(Vector3(1.0, 0.0, 0.0));
    });
    _fillGButton->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) {
        fillTargetWithColour(Vector3(0.0, 1.0, 0.0));
    });
    _fillBButton->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) {
        fillTargetWithColour(Vector3(0.0, 0.0, 1.0));
    });
    _clearButton->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) {
        fillTargetWithColour(Vector3(0.0, 0.0, 0.0));
    });
}

void VertexPaintPanel::pullFromSettings()
{
    auto& s = VertexPaintSettings::Instance();
    _channelChoice->SetSelection(static_cast<int>(s.channel));
    _previewModeChoice->SetSelection(static_cast<int>(s.previewMode));
    _falloffTypeChoice->SetSelection(static_cast<int>(s.falloffType));
    _radius->SetValue(s.radius);
    _strength->SetValue(s.strength);
    _falloff->SetValue(s.falloff);
}

void VertexPaintPanel::pushToSettings()
{
    auto& s = VertexPaintSettings::Instance();
    int ch = _channelChoice->GetSelection();
    if (ch >= 0) s.channel = static_cast<VertexPaintChannel>(ch);
    int pm = _previewModeChoice->GetSelection();
    if (pm >= 0) s.previewMode = static_cast<model::PaintablePreviewMode>(pm);
    int ft = _falloffTypeChoice->GetSelection();
    if (ft >= 0) s.falloffType = static_cast<VertexBrushFalloff>(ft);
    s.radius = static_cast<float>(_radius->GetValue());
    s.strength = static_cast<float>(_strength->GetValue());
    s.falloff = static_cast<float>(_falloff->GetValue());
}

void VertexPaintPanel::onChoiceChange(wxCommandEvent&)
{
    pushToSettings();
}

void VertexPaintPanel::onPreviewModeChanged(wxCommandEvent&)
{
    pushToSettings();
    applyPreviewModeToScene(VertexPaintSettings::Instance().previewMode);
    GlobalMainFrame().updateAllWindows();
}

void VertexPaintPanel::onSpinChange(wxSpinDoubleEvent&)
{
    pushToSettings();
}

void VertexPaintPanel::fillTargetWithColour(const Vector3& colour)
{
    scene::INodePtr target = VertexPaintSettings::Instance().target.lock();
    if (!target)
    {
        wxutil::Messagebox::ShowError(
            _("Hover over a paintable model in the 3D view first."), this);
        return;
    }

    auto* paintable = Node_getVertexPaintable(target);
    if (!paintable) return;

    std::string modelPath = paintable->getPaintableModelPath();
    if (modelPath.empty()) return;

    auto root = GlobalSceneGraph().root();
    if (!root) return;

    FillWalker walker;
    walker.modelPath = modelPath;
    walker.colour = colour;
    root->traverse(walker);

    GlobalMainFrame().updateAllWindows();
}

void VertexPaintPanel::onSaveClicked(wxCommandEvent&)
{
    scene::INodePtr target = VertexPaintSettings::Instance().target.lock();
    if (!target)
    {
        wxutil::Messagebox::ShowError(
            _("Hover over a paintable model in the 3D view first."), this);
        return;
    }

    auto* paintable = Node_getVertexPaintable(target);
    if (!paintable)
    {
        wxutil::Messagebox::ShowError(_("Target is not paintable."), this);
        return;
    }

    std::string modelPath = paintable->getPaintableModelPath();
    std::string absolute = resolveAbsolutePath(modelPath);
    if (absolute.empty())
    {
        wxutil::Messagebox::ShowError(
            _("Cannot save: model was loaded from an archive (pak) and has no writable path on disk."),
            this);
        return;
    }

    auto exporter = GlobalModelFormatManager().getExporter("ASE");
    if (!exporter)
    {
        wxutil::Messagebox::ShowError(_("No ASE exporter available."), this);
        return;
    }
    exporter = exporter->clone();

    std::size_t count = paintable->getPaintableSurfaceCount();
    for (std::size_t i = 0; i < count; ++i)
    {
        if (auto* surf = paintable->getPaintableSurface(i))
        {
            exporter->addSurface(*surf, Matrix4::getIdentity());
        }
    }

    try
    {
        fs::path p(absolute);
        std::string outDir = p.parent_path().string();
        std::string filename = p.filename().string();
        exporter->exportToPath(outDir, filename);
    }
    catch (const std::exception& e)
    {
        wxutil::Messagebox::ShowError(
            std::string(_("Failed to save: ")) + e.what(), this);
        return;
    }

    wxutil::Messagebox::Show(_("Vertex Paint"),
        std::string(_("Saved: ")) + absolute,
        IDialog::MESSAGE_CONFIRM, this);
}

void VertexPaintPanel::onPanelActivated()
{
    pullFromSettings();

    if (_preview && !_previewAttached)
    {
        GlobalRenderSystem().attachRenderable(*_preview);
        _previewAttached = true;
    }

    auto& s = VertexPaintSettings::Instance();
    s.panelActive = true;
    applyPreviewModeToScene(s.previewMode);
}

void VertexPaintPanel::onPanelDeactivated()
{
    if (_preview && _previewAttached)
    {
        _preview->clear();
        GlobalRenderSystem().detachRenderable(*_preview);
        _previewAttached = false;
    }

    applyPreviewModeToScene(model::PaintablePreviewMode::Material);

    auto& s = VertexPaintSettings::Instance();
    s.panelActive = false;
    s.hoverValid = false;
    s.target.reset();
    GlobalMainFrame().updateAllWindows();
}

}
