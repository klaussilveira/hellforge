#include "HotspotEditor.h"

#include "i18n.h"
#include "iimage.h"
#include "imap.h"
#include "ishaderclipboard.h"
#include "itextstream.h"
#include "ui/imainframe.h"
#include "hotspot/HotspotDetect.h"
#include "hotspot/HotspotFit.h"
#include "hotspot/HotspotLookup.h"
#include "ui/materials/MaterialChooser.h"
#include "ui/materials/MaterialSelector.h"
#include "wxutil/PickerButton.h"

#include <fmt/format.h>

#include <wx/button.h>
#include <wx/checkbox.h>
#include <wx/choice.h>
#include <wx/dcbuffer.h>
#include <wx/panel.h>
#include <wx/sizer.h>
#include <wx/slider.h>
#include <wx/spinctrl.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>

#include <algorithm>
#include <cmath>
#include <functional>

namespace ui
{

namespace
{

const int GRID_SIZES[] = { 1, 2, 4, 8, 16, 32 };
const int HANDLE_TOLERANCE = 5;

enum Edge
{
    EDGE_LEFT = 1,
    EDGE_RIGHT = 2,
    EDGE_TOP = 4,
    EDGE_BOTTOM = 8,
};

}

class HotspotCanvas :
    public wxPanel
{
private:
    hotspot::Sheet& _sheet;
    int& _selected;
    const int& _grid;

    wxImage _image;
    wxBitmap _bitmap;
    double _zoom = 1;
    wxPoint _offset;

    enum class Drag { None, Create, Move, Resize } _drag = Drag::None;
    int _edges = 0;
    hotspot::Rect _startRect;
    hotspot::Rect _createRect;
    double _startX = 0;
    double _startY = 0;

public:
    std::function<void()> onChanged;

    HotspotCanvas(wxWindow* parent, hotspot::Sheet& sheet, int& selected, const int& grid) :
        wxPanel(parent, wxID_ANY, wxDefaultPosition, wxSize(512, 512), wxWANTS_CHARS),
        _sheet(sheet),
        _selected(selected),
        _grid(grid)
    {
        SetBackgroundStyle(wxBG_STYLE_PAINT);

        Bind(wxEVT_PAINT, &HotspotCanvas::onPaint, this);
        Bind(wxEVT_SIZE, [this](wxSizeEvent& ev) { _bitmap = wxBitmap(); Refresh(); ev.Skip(); });
        Bind(wxEVT_LEFT_DOWN, &HotspotCanvas::onLeftDown, this);
        Bind(wxEVT_LEFT_UP, &HotspotCanvas::onLeftUp, this);
        Bind(wxEVT_MOTION, &HotspotCanvas::onMotion, this);
        Bind(wxEVT_KEY_DOWN, &HotspotCanvas::onKeyDown, this);
        Bind(wxEVT_MOUSE_CAPTURE_LOST, [this](wxMouseCaptureLostEvent&) { _drag = Drag::None; });
    }

    void setImage(const wxImage& image)
    {
        _image = image;
        _bitmap = wxBitmap();
        Refresh();
    }

private:
    void updateTransform()
    {
        auto size = GetClientSize();

        if (_sheet.width <= 0 || _sheet.height <= 0)
        {
            _zoom = 1;
            _offset = wxPoint(0, 0);
            return;
        }

        _zoom = std::min(static_cast<double>(size.x) / _sheet.width, static_cast<double>(size.y) / _sheet.height);
        _offset = wxPoint(static_cast<int>((size.x - _sheet.width * _zoom) / 2),
            static_cast<int>((size.y - _sheet.height * _zoom) / 2));
    }

    wxRect toScreen(const hotspot::Rect& rect) const
    {
        return wxRect(
            wxPoint(_offset.x + static_cast<int>(std::lround(rect.minX * _zoom)),
                    _offset.y + static_cast<int>(std::lround(rect.minY * _zoom))),
            wxPoint(_offset.x + static_cast<int>(std::lround(rect.maxX * _zoom)) - 1,
                    _offset.y + static_cast<int>(std::lround(rect.maxY * _zoom)) - 1));
    }

    double toSheetX(int x) const { return (x - _offset.x) / _zoom; }
    double toSheetY(int y) const { return (y - _offset.y) / _zoom; }

    int snap(double value, int limit) const
    {
        return std::clamp(static_cast<int>(std::lround(value / _grid)) * _grid, 0, limit);
    }

    int hitEdges(const hotspot::Rect& rect, const wxPoint& point) const
    {
        auto screen = toScreen(rect);

        if (!screen.Inflate(HANDLE_TOLERANCE).Contains(point))
        {
            return 0;
        }

        screen = toScreen(rect);
        int edges = 0;

        if (std::abs(point.x - screen.GetLeft()) <= HANDLE_TOLERANCE) edges |= EDGE_LEFT;
        else if (std::abs(point.x - screen.GetRight()) <= HANDLE_TOLERANCE) edges |= EDGE_RIGHT;

        if (std::abs(point.y - screen.GetTop()) <= HANDLE_TOLERANCE) edges |= EDGE_TOP;
        else if (std::abs(point.y - screen.GetBottom()) <= HANDLE_TOLERANCE) edges |= EDGE_BOTTOM;

        return edges;
    }

    bool hasSelection() const
    {
        return _selected >= 0 && _selected < static_cast<int>(_sheet.rects.size());
    }

    void onPaint(wxPaintEvent&)
    {
        wxAutoBufferedPaintDC dc(this);
        dc.SetBackground(wxBrush(wxColour(48, 48, 48)));
        dc.Clear();

        updateTransform();

        if (_image.IsOk() && _sheet.width > 0 && _sheet.height > 0)
        {
            auto width = std::max(1, static_cast<int>(std::lround(_sheet.width * _zoom)));
            auto height = std::max(1, static_cast<int>(std::lround(_sheet.height * _zoom)));

            if (!_bitmap.IsOk() || _bitmap.GetWidth() != width || _bitmap.GetHeight() != height)
            {
                _bitmap = wxBitmap(_image.Scale(width, height, wxIMAGE_QUALITY_BILINEAR));
            }

            dc.DrawBitmap(_bitmap, _offset);
        }

        dc.SetBrush(*wxTRANSPARENT_BRUSH);

        for (std::size_t i = 0; i < _sheet.rects.size(); ++i)
        {
            const auto& rect = _sheet.rects[i];
            bool tiles = hotspot::spansWidth(_sheet, rect) || hotspot::spansHeight(_sheet, rect);

            wxColour colour = static_cast<int>(i) == _selected ? wxColour(255, 220, 0) :
                rect.alt ? wxColour(0, 200, 255) : wxColour(80, 255, 80);

            dc.SetPen(wxPen(colour, static_cast<int>(i) == _selected ? 2 : 1,
                tiles ? wxPENSTYLE_SHORT_DASH : wxPENSTYLE_SOLID));
            dc.DrawRectangle(toScreen(rect));
        }

        if (hasSelection())
        {
            auto screen = toScreen(_sheet.rects[_selected]);
            dc.SetBrush(wxBrush(wxColour(255, 220, 0)));
            dc.SetPen(*wxBLACK_PEN);

            for (const auto& corner : { screen.GetTopLeft(), screen.GetTopRight(), screen.GetBottomLeft(), screen.GetBottomRight() })
            {
                dc.DrawRectangle(corner.x - 3, corner.y - 3, 7, 7);
            }
        }

        if (_drag == Drag::Create)
        {
            dc.SetBrush(*wxTRANSPARENT_BRUSH);
            dc.SetPen(wxPen(wxColour(255, 255, 255), 1, wxPENSTYLE_DOT));
            dc.DrawRectangle(toScreen(_createRect));
        }
    }

    void onLeftDown(wxMouseEvent& ev)
    {
        SetFocus();

        if (_sheet.width <= 0 || _sheet.height <= 0)
        {
            return;
        }

        _startX = toSheetX(ev.GetX());
        _startY = toSheetY(ev.GetY());
        _edges = hasSelection() ? hitEdges(_sheet.rects[_selected], ev.GetPosition()) : 0;

        if (_edges != 0)
        {
            _drag = Drag::Resize;
        }
        else
        {
            _selected = -1;

            for (auto i = static_cast<int>(_sheet.rects.size()) - 1; i >= 0; --i)
            {
                const auto& rect = _sheet.rects[i];

                if (_startX >= rect.minX && _startX < rect.maxX && _startY >= rect.minY && _startY < rect.maxY)
                {
                    _selected = i;
                    break;
                }
            }

            _drag = hasSelection() ? Drag::Move : Drag::Create;
            _createRect = hotspot::Rect();
        }

        if (hasSelection())
        {
            _startRect = _sheet.rects[_selected];
        }

        CaptureMouse();

        if (onChanged) onChanged();
        Refresh();
    }

    void onMotion(wxMouseEvent& ev)
    {
        if (_drag == Drag::None)
        {
            int edges = hasSelection() ? hitEdges(_sheet.rects[_selected], ev.GetPosition()) : 0;

            SetCursor(wxCursor(
                (edges & (EDGE_LEFT | EDGE_RIGHT)) && (edges & (EDGE_TOP | EDGE_BOTTOM)) ? wxCURSOR_SIZING :
                (edges & (EDGE_LEFT | EDGE_RIGHT)) ? wxCURSOR_SIZEWE :
                (edges & (EDGE_TOP | EDGE_BOTTOM)) ? wxCURSOR_SIZENS : wxCURSOR_ARROW));
            return;
        }

        auto x = toSheetX(ev.GetX());
        auto y = toSheetY(ev.GetY());

        if (_drag == Drag::Create)
        {
            auto x0 = snap(_startX, _sheet.width);
            auto y0 = snap(_startY, _sheet.height);
            auto x1 = snap(x, _sheet.width);
            auto y1 = snap(y, _sheet.height);

            _createRect.minX = std::min(x0, x1);
            _createRect.maxX = std::max(x0, x1);
            _createRect.minY = std::min(y0, y1);
            _createRect.maxY = std::max(y0, y1);
        }
        else if (_drag == Drag::Move && hasSelection())
        {
            auto& rect = _sheet.rects[_selected];
            auto dx = std::clamp(snap(x - _startX + _startRect.minX, _sheet.width) - _startRect.minX,
                -_startRect.minX, _sheet.width - _startRect.maxX);
            auto dy = std::clamp(snap(y - _startY + _startRect.minY, _sheet.height) - _startRect.minY,
                -_startRect.minY, _sheet.height - _startRect.maxY);

            rect.minX = _startRect.minX + dx;
            rect.maxX = _startRect.maxX + dx;
            rect.minY = _startRect.minY + dy;
            rect.maxY = _startRect.maxY + dy;
        }
        else if (_drag == Drag::Resize && hasSelection())
        {
            auto& rect = _sheet.rects[_selected];
            rect = _startRect;

            if (_edges & EDGE_LEFT) rect.minX = snap(x, _sheet.width);
            if (_edges & EDGE_RIGHT) rect.maxX = snap(x, _sheet.width);
            if (_edges & EDGE_TOP) rect.minY = snap(y, _sheet.height);
            if (_edges & EDGE_BOTTOM) rect.maxY = snap(y, _sheet.height);
        }

        Refresh();
    }

    void onLeftUp(wxMouseEvent&)
    {
        if (HasCapture())
        {
            ReleaseMouse();
        }

        if (_drag == Drag::Create && _createRect.width() > 0 && _createRect.height() > 0)
        {
            _sheet.rects.push_back(_createRect);
            _selected = static_cast<int>(_sheet.rects.size()) - 1;
        }
        else if (_drag == Drag::Resize && hasSelection())
        {
            auto& rect = _sheet.rects[_selected];

            if (rect.minX > rect.maxX) std::swap(rect.minX, rect.maxX);
            if (rect.minY > rect.maxY) std::swap(rect.minY, rect.maxY);

            if (rect.width() == 0 || rect.height() == 0)
            {
                rect = _startRect;
            }
        }

        _drag = Drag::None;

        if (onChanged) onChanged();
        Refresh();
    }

    void onKeyDown(wxKeyEvent& ev)
    {
        if ((ev.GetKeyCode() == WXK_DELETE || ev.GetKeyCode() == WXK_BACK) && hasSelection())
        {
            _sheet.rects.erase(_sheet.rects.begin() + _selected);
            _selected = -1;

            if (onChanged) onChanged();
            Refresh();
            return;
        }

        ev.Skip();
    }
};

HotspotEditor::HotspotEditor(const std::string& material) :
    DialogBase(_("Hotspot Rect Editor"), GlobalMainFrame().getWxTopLevelWindow())
{
    auto* mainSizer = new wxBoxSizer(wxHORIZONTAL);

    _canvas = new HotspotCanvas(this, _sheet, _selected, _grid);
    _canvas->onChanged = [this]() { updateControls(); };
    mainSizer->Add(_canvas, 1, wxEXPAND | wxALL, 8);

    auto* side = new wxBoxSizer(wxVERTICAL);

    side->Add(new wxStaticText(this, wxID_ANY, _("Material:")), 0, wxBOTTOM, 4);

    auto* materialRow = new wxBoxSizer(wxHORIZONTAL);
    _materialEntry = new wxTextCtrl(this, wxID_ANY, "", wxDefaultPosition, wxSize(260, -1), wxTE_PROCESS_ENTER);
    _materialEntry->Bind(wxEVT_TEXT_ENTER, [this](wxCommandEvent&) { loadMaterial(_materialEntry->GetValue().ToStdString()); });
    materialRow->Add(_materialEntry, 1, wxEXPAND | wxRIGHT, 4);

    auto* browse = wxutil::PickerButton(this);
    browse->Bind(wxEVT_BUTTON, &HotspotEditor::onBrowseMaterial, this);
    materialRow->Add(browse, 0);
    side->Add(materialRow, 0, wxEXPAND | wxBOTTOM, 6);

    _infoLabel = new wxStaticText(this, wxID_ANY, "");
    side->Add(_infoLabel, 0, wxEXPAND | wxBOTTOM, 10);

    auto* grid = new wxFlexGridSizer(2, 6, 8);
    grid->AddGrowableCol(1);

    grid->Add(new wxStaticText(this, wxID_ANY, _("Units per pixel:")), 0, wxALIGN_CENTER_VERTICAL);
    _scale = new wxSpinCtrlDouble(this, wxID_ANY);
    _scale->SetRange(0, 64);
    _scale->SetIncrement(0.125);
    _scale->SetDigits(3);
    _scale->SetToolTip(_("World units per pixel of this sheet. 0 uses the default texture scale."));
    _scale->Bind(wxEVT_SPINCTRLDOUBLE, [this](wxSpinDoubleEvent&) { _sheet.scale = _scale->GetValue(); });
    grid->Add(_scale, 0, wxEXPAND);

    grid->Add(new wxStaticText(this, wxID_ANY, _("Inset (pixels):")), 0, wxALIGN_CENTER_VERTICAL);
    _inset = new wxSpinCtrlDouble(this, wxID_ANY);
    _inset->SetRange(0, 64);
    _inset->SetIncrement(0.5);
    _inset->SetDigits(1);
    _inset->Bind(wxEVT_SPINCTRLDOUBLE, [this](wxSpinDoubleEvent&) { _sheet.inset = _inset->GetValue(); });
    grid->Add(_inset, 0, wxEXPAND);

    grid->Add(new wxStaticText(this, wxID_ANY, _("Snap grid:")), 0, wxALIGN_CENTER_VERTICAL);
    _gridChoice = new wxChoice(this, wxID_ANY);

    for (auto size : GRID_SIZES)
    {
        _gridChoice->Append(std::to_string(size));
    }

    _gridChoice->SetSelection(3);
    _gridChoice->Bind(wxEVT_CHOICE, [this](wxCommandEvent&) { _grid = GRID_SIZES[_gridChoice->GetSelection()]; });
    grid->Add(_gridChoice, 0, wxEXPAND);

    grid->Add(new wxStaticText(this, wxID_ANY, _("Detect sensitivity:")), 0, wxALIGN_CENTER_VERTICAL);
    _consistency = new wxSlider(this, wxID_ANY, 50, 0, 100);
    _consistency->SetToolTip(_("Higher values find fainter seams but also more false cuts."));
    grid->Add(_consistency, 0, wxEXPAND);

    side->Add(grid, 0, wxEXPAND | wxBOTTOM, 8);

    auto* detectButton = new wxButton(this, wxID_ANY, _("Detect Layout"));
    detectButton->SetToolTip(_("Guess the rectangles from the seams in the image. Replaces the current rectangles."));
    detectButton->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { detect(); });
    side->Add(detectButton, 0, wxEXPAND | wxBOTTOM, 4);

    auto* clearButton = new wxButton(this, wxID_ANY, _("Clear All"));
    clearButton->Bind(wxEVT_BUTTON, [this](wxCommandEvent&)
    {
        _sheet.rects.clear();
        _selected = -1;
        updateControls();
        _canvas->Refresh();
    });
    side->Add(clearButton, 0, wxEXPAND | wxBOTTOM, 12);

    side->Add(new wxStaticText(this, wxID_ANY, _("Selected rectangle:")), 0, wxBOTTOM, 4);

    _rotate = new wxCheckBox(this, wxID_ANY, _("Allow rotation"));
    _reflect = new wxCheckBox(this, wxID_ANY, _("Allow reflection"));
    _alt = new wxCheckBox(this, wxID_ANY, _("Alternate (Alt hotspot only)"));

    for (auto* box : { _rotate, _reflect, _alt })
    {
        box->Bind(wxEVT_CHECKBOX, &HotspotEditor::onFlagChanged, this);
        side->Add(box, 0, wxBOTTOM, 4);
    }

    auto* deleteButton = new wxButton(this, wxID_ANY, _("Delete Rectangle"));
    deleteButton->Bind(wxEVT_BUTTON, [this](wxCommandEvent&)
    {
        if (_selected >= 0 && _selected < static_cast<int>(_sheet.rects.size()))
        {
            _sheet.rects.erase(_sheet.rects.begin() + _selected);
            _selected = -1;
            updateControls();
            _canvas->Refresh();
        }
    });
    side->Add(deleteButton, 0, wxEXPAND | wxTOP | wxBOTTOM, 4);

    side->AddStretchSpacer();

    _mapOnly = new wxCheckBox(this, wxID_ANY, _("Use for this map only"));
    _mapOnly->SetToolTip(_("Save next to the map and reference it from the map's .darkradiant file."));
    side->Add(_mapOnly, 0, wxBOTTOM, 8);

    auto* buttons = new wxBoxSizer(wxHORIZONTAL);

    auto* revertButton = new wxButton(this, wxID_ANY, _("Revert"));
    revertButton->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { loadMaterial(_material); });
    buttons->Add(revertButton, 0, wxRIGHT, 4);

    auto* saveButton = new wxButton(this, wxID_SAVE, _("Save"));
    saveButton->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { save(); });
    buttons->Add(saveButton, 0, wxRIGHT, 4);

    buttons->Add(new wxButton(this, wxID_CANCEL, _("Close")), 0);

    side->Add(buttons, 0, wxALIGN_RIGHT);

    mainSizer->Add(side, 0, wxEXPAND | wxTOP | wxRIGHT | wxBOTTOM, 8);

    SetSizer(mainSizer);
    FitToScreen(0.8f, 0.8f);
    CenterOnParent();

    loadMaterial(material);
}

void HotspotEditor::loadMaterial(const std::string& material)
{
    _material = material;
    _materialEntry->ChangeValue(material);
    _selected = -1;
    _sheet = hotspot::Sheet();
    _sourcePath.clear();

    loadImage();

    if (auto loaded = hotspot::findSheet(material); loaded)
    {
        _sheet = loaded->sheet;
        _sourcePath = loaded->path;
    }
    else
    {
        _sheet.width = _imageWidth;
        _sheet.height = _imageHeight;
    }

    _scale->SetValue(_sheet.scale);
    _inset->SetValue(_sheet.inset);

    bool hasMap = !GlobalMapModule().isUnnamed();
    _mapOnly->Enable(hasMap);
    _mapOnly->SetValue(hasMap && !_sourcePath.empty() && _sourcePath == hotspot::getMapOverridePath(material));

    updateControls();
    _canvas->Refresh();
}

void HotspotEditor::loadImage()
{
    _pixels.clear();
    _imageWidth = _imageHeight = 0;

    auto imagePath = hotspot::getEditorImagePath(_material);
    auto image = imagePath.empty() ? ImagePtr() : GlobalImageLoader().imageFromVFS(imagePath);

    if (!image || image->isPrecompressed())
    {
        _canvas->setImage(wxImage());
        return;
    }

    _imageWidth = static_cast<int>(image->getWidth());
    _imageHeight = static_cast<int>(image->getHeight());

    const auto* source = image->getPixels();
    _pixels.assign(source, source + static_cast<std::size_t>(_imageWidth) * _imageHeight * 4);

    wxImage display(_imageWidth, _imageHeight, false);
    auto* target = display.GetData();

    for (std::size_t i = 0; i < static_cast<std::size_t>(_imageWidth) * _imageHeight; ++i)
    {
        target[i * 3] = _pixels[i * 4];
        target[i * 3 + 1] = _pixels[i * 4 + 1];
        target[i * 3 + 2] = _pixels[i * 4 + 2];
    }

    _canvas->setImage(display);
}

void HotspotEditor::updateControls()
{
    bool hasSelection = _selected >= 0 && _selected < static_cast<int>(_sheet.rects.size());

    for (auto* box : { _rotate, _reflect, _alt })
    {
        box->Enable(hasSelection);
    }

    _rotate->SetValue(hasSelection && _sheet.rects[_selected].rotate);
    _reflect->SetValue(hasSelection && _sheet.rects[_selected].reflect);
    _alt->SetValue(hasSelection && _sheet.rects[_selected].alt);

    updateInfo();
}

void HotspotEditor::updateInfo()
{
    std::string info;

    if (_imageWidth == 0)
    {
        info = _("The editor image could not be displayed.");
    }
    else
    {
        info = fmt::format(_("Image: {0} x {1}"), _imageWidth, _imageHeight);
    }

    info += "\n" + fmt::format(_("Sheet resolution: {0} x {1}"), _sheet.width, _sheet.height);
    info += "\n" + fmt::format(_("Rectangles: {0}"), _sheet.rects.size());
    info += "\n" + (_sourcePath.empty() ? std::string(_("No rect file yet")) : fmt::format(_("File: {0}"), _sourcePath));

    if (_selected >= 0 && _selected < static_cast<int>(_sheet.rects.size()))
    {
        const auto& rect = _sheet.rects[_selected];
        info += "\n" + fmt::format(_("Selected: {0},{1} to {2},{3} ({4} x {5})"),
            rect.minX, rect.minY, rect.maxX, rect.maxY, rect.width(), rect.height());
    }

    _infoLabel->SetLabel(info);
    Layout();
}

void HotspotEditor::detect()
{
    if (_pixels.empty() || _sheet.width <= 0 || _sheet.height <= 0)
    {
        return;
    }

    double consistency = 0.98 - 0.16 * _consistency->GetValue() / 100.0;

    auto rects = hotspot::detectRects(_pixels.data(), _imageWidth, _imageHeight, consistency);

    for (auto& rect : rects)
    {
        rect.minX = static_cast<int>(std::lround(static_cast<double>(rect.minX) * _sheet.width / _imageWidth));
        rect.maxX = static_cast<int>(std::lround(static_cast<double>(rect.maxX) * _sheet.width / _imageWidth));
        rect.minY = static_cast<int>(std::lround(static_cast<double>(rect.minY) * _sheet.height / _imageHeight));
        rect.maxY = static_cast<int>(std::lround(static_cast<double>(rect.maxY) * _sheet.height / _imageHeight));
    }

    _sheet.rects = hotspot::snapRects(rects, _grid, _sheet.width, _sheet.height);
    _selected = -1;

    updateControls();
    _canvas->Refresh();
}

void HotspotEditor::save()
{
    if (_material.empty() || _sheet.width <= 0 || _sheet.height <= 0)
    {
        return;
    }

    auto overridePath = hotspot::getMapOverridePath(_material);
    std::string path;

    if (_mapOnly->GetValue())
    {
        path = hotspot::getMapOnlyPath(_material);
    }
    else if (!_sourcePath.empty() && _sourcePath != overridePath)
    {
        path = _sourcePath;
    }
    else
    {
        path = _material + hotspot::RECT_EXTENSION;
    }

    if (!hotspot::saveSheetFile(_sheet, path))
    {
        return;
    }

    if (auto root = GlobalMapModule().getRoot(); root)
    {
        auto key = hotspot::MAP_PROPERTY_PREFIX + _material;

        if (_mapOnly->GetValue() && overridePath != path)
        {
            root->setProperty(key, path);
            GlobalMapModule().setModified(true);
        }
        else if (!_mapOnly->GetValue() && !overridePath.empty())
        {
            root->removeProperty(key);
            GlobalMapModule().setModified(true);
        }
    }

    _sourcePath = path;
    rMessage() << "Saved hotspot rects to " << hotspot::getAbsoluteWritePath(path) << std::endl;

    updateInfo();
}

void HotspotEditor::onBrowseMaterial(wxCommandEvent& ev)
{
    MaterialChooser chooser(this, MaterialSelector::TextureFilter::Regular, _materialEntry);
    chooser.ShowModal();

    auto material = _materialEntry->GetValue().ToStdString();

    if (material != _material)
    {
        loadMaterial(material);
    }
}

void HotspotEditor::onFlagChanged(wxCommandEvent& ev)
{
    if (_selected < 0 || _selected >= static_cast<int>(_sheet.rects.size()))
    {
        return;
    }

    auto& rect = _sheet.rects[_selected];
    rect.rotate = _rotate->GetValue();
    rect.reflect = _reflect->GetValue();
    rect.alt = _alt->GetValue();

    _canvas->Refresh();
}

void HotspotEditor::Show(const cmd::ArgumentList& args)
{
    auto material = !args.empty() ? args[0].getString() : GlobalShaderClipboard().getShaderName();

    HotspotEditor editor(material);
    editor.ShowModal();
}

}
