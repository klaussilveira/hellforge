#include "AssetBrowserPanel.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <set>

#include <wx/checkbox.h>
#include <wx/choice.h>
#include <wx/control.h>
#include <wx/dcbuffer.h>
#include <wx/dnd.h>
#include <wx/menu.h>
#include <wx/scrolwin.h>
#include <wx/settings.h>
#include <wx/sizer.h>
#include <wx/textctrl.h>

#include "i18n.h"
#include "icameraview.h"
#include "ideclmanager.h"
#include "ieclass.h"
#include "ifilesystem.h"
#include "igame.h"
#include "imodelcache.h"
#include "ipreferencesystem.h"
#include "registry/registry.h"
#include "ui/iuserinterface.h"
#include "HiddenModelFilter.h"
#include "os/path.h"
#include "string/case_conv.h"
#include "string/convert.h"
#include "string/split.h"

#include "AssetDropTarget.h"
#include "AssetTypes.h"
#include "ThumbnailCache.h"
#include "ThumbnailPreview.h"
#include "ThumbnailViewStore.h"

namespace ui
{

namespace
{

constexpr int TILE_PADDING = 6;
constexpr int LABEL_HEIGHT = 16;
constexpr int DRAG_THRESHOLD = 6;
constexpr int DEFAULT_THUMBNAIL_SIZE = 128;
constexpr float DEFAULT_THUMBNAIL_PADDING = 1.1f;
constexpr int THUMBNAIL_SUPERSAMPLE = 2;
constexpr double ORBIT_SPEED = 0.6;

constexpr int MENU_USE_PREVIEW_ANGLE = wxID_HIGHEST + 1;
constexpr int MENU_RESET_VIEW = wxID_HIGHEST + 2;
constexpr int MENU_REBUILD = wxID_HIGHEST + 3;
constexpr int MENU_REBUILD_ALL = wxID_HIGHEST + 4;

const char* const RKEY_THUMBNAIL_SIZE = "user/ui/assetBrowser/thumbnailSize";
const char* const RKEY_THUMBNAIL_PADDING = "user/ui/assetBrowser/thumbnailPadding";
const char* const RKEY_SHOW_PREVIEW = "user/ui/assetBrowser/showPreview";

struct AssetTile
{
    std::string type;
    std::string name;
    std::string label;
    std::string key;
};

}

class AssetBrowserGrid :
    public wxScrolledWindow
{
public:
    AssetBrowserGrid(wxWindow* parent, ThumbnailCache& cache, AssetBrowserPanel& owner) :
        wxScrolledWindow(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxBORDER_SUNKEN | wxVSCROLL),
        _cache(cache),
        _owner(owner)
    {
        SetBackgroundStyle(wxBG_STYLE_PAINT);
        SetScrollRate(0, 24);

        Bind(wxEVT_PAINT, &AssetBrowserGrid::onPaint, this);
        Bind(wxEVT_SIZE, &AssetBrowserGrid::onSize, this);
        Bind(wxEVT_LEFT_DOWN, &AssetBrowserGrid::onLeftDown, this);
        Bind(wxEVT_LEFT_UP, &AssetBrowserGrid::onLeftUp, this);
        Bind(wxEVT_RIGHT_DOWN, &AssetBrowserGrid::onRightDown, this);
        Bind(wxEVT_RIGHT_UP, &AssetBrowserGrid::onRightUp, this);
        Bind(wxEVT_MOTION, &AssetBrowserGrid::onMotion, this);
    }

    void setThumbnailSize(int size)
    {
        _thumbSize = size;

        updateLayout();
        Refresh();
    }

    void setTiles(std::vector<AssetTile> tiles)
    {
        _tiles = std::move(tiles);
        _selected = -1;
        _dragIndex = -1;
        _orbitIndex = -1;

        updateLayout();
        Scroll(0, 0);
        Refresh();
    }

    const AssetTile* getSelectedTile() const
    {
        return _selected >= 0 && _selected < static_cast<int>(_tiles.size())
            ? &_tiles[_selected] : nullptr;
    }

    bool isKeyVisible(const std::string& key)
    {
        int first, last;
        getVisibleRange(first, last);

        for (int index = first; index <= last && index < static_cast<int>(_tiles.size()); ++index)
        {
            if (_tiles[index].key == key) return true;
        }

        return false;
    }

private:
    int tileWidth() const
    {
        return _thumbSize + 2 * TILE_PADDING;
    }

    int tileHeight() const
    {
        return TILE_PADDING + _thumbSize + 4 + LABEL_HEIGHT + TILE_PADDING;
    }

    int getColumns() const
    {
        return std::max(1, GetClientSize().GetWidth() / tileWidth());
    }

    void getVisibleRange(int& first, int& last)
    {
        int columns = getColumns();
        auto viewStart = CalcUnscrolledPosition(wxPoint(0, 0));
        int clientHeight = GetClientSize().GetHeight();

        int firstRow = std::max(0, viewStart.y / tileHeight());
        int lastRow = std::max(0, (viewStart.y + clientHeight) / tileHeight());

        first = firstRow * columns;
        last = lastRow * columns + columns - 1;
    }

    void updateLayout()
    {
        int columns = getColumns();
        int rows = (static_cast<int>(_tiles.size()) + columns - 1) / columns;

        SetVirtualSize(GetClientSize().GetWidth(), rows * tileHeight());
    }

    int hitTest(const wxPoint& windowPoint)
    {
        auto point = CalcUnscrolledPosition(windowPoint);

        int columns = getColumns();
        int column = point.x / tileWidth();
        int row = point.y / tileHeight();

        if (point.x < 0 || point.y < 0 || column >= columns) return -1;

        int index = row * columns + column;

        return index < static_cast<int>(_tiles.size()) ? index : -1;
    }

    void onPaint(wxPaintEvent&)
    {
        wxAutoBufferedPaintDC dc(this);
        DoPrepareDC(dc);

        dc.SetBackground(wxBrush(wxSystemSettings::GetColour(wxSYS_COLOUR_LISTBOX)));
        dc.Clear();

        if (_tiles.empty()) return;

        int columns = getColumns();

        int first, last;
        getVisibleRange(first, last);

        dc.SetFont(GetFont());

        for (int index = first; index <= last && index < static_cast<int>(_tiles.size()); ++index)
        {
            int x = (index % columns) * tileWidth();
            int y = (index / columns) * tileHeight();

            drawTile(dc, index, x, y);
        }

        if (_cache.hasPendingRenders())
        {
            _owner.requestIdleCallback();
        }
    }

    void drawTile(wxDC& dc, int index, int x, int y)
    {
        const auto& tile = _tiles[index];
        bool selected = index == _selected;

        if (selected)
        {
            dc.SetBrush(wxBrush(wxSystemSettings::GetColour(wxSYS_COLOUR_HIGHLIGHT)));
            dc.SetPen(*wxTRANSPARENT_PEN);
            dc.DrawRectangle(x, y, tileWidth(), tileHeight());
        }

        int thumbX = x + (tileWidth() - _thumbSize) / 2;
        int thumbY = y + TILE_PADDING;

        const auto* bitmap = _cache.find(tile.key);

        if (bitmap != nullptr)
        {
            dc.DrawBitmap(*bitmap, thumbX, thumbY, true);
        }
        else
        {
            dc.SetBrush(wxBrush(wxSystemSettings::GetColour(wxSYS_COLOUR_BTNFACE)));
            dc.SetPen(wxPen(wxSystemSettings::GetColour(wxSYS_COLOUR_BTNSHADOW)));
            dc.DrawRectangle(thumbX, thumbY, _thumbSize, _thumbSize);

            _cache.request(tile.key, _owner.getCacheVariant(tile.key),
                tile.type == assetType::Model ? tile.name : std::string());
        }

        dc.SetTextForeground(wxSystemSettings::GetColour(
            selected ? wxSYS_COLOUR_HIGHLIGHTTEXT : wxSYS_COLOUR_LISTBOXTEXT));

        wxString label = wxControl::Ellipsize(tile.label, dc, wxELLIPSIZE_MIDDLE, tileWidth() - 8);
        wxSize labelSize = dc.GetTextExtent(label);

        dc.DrawText(label, x + (tileWidth() - labelSize.GetWidth()) / 2, thumbY + _thumbSize + 4);
    }

    void onSize(wxSizeEvent& ev)
    {
        updateLayout();
        Refresh();
        ev.Skip();
    }

    void onLeftDown(wxMouseEvent& ev)
    {
        int index = hitTest(ev.GetPosition());

        _selected = index;
        _dragIndex = index;
        _dragStart = ev.GetPosition();

        Refresh();

        if (index != -1)
        {
            _owner.showSelectedInPreview();
        }
    }

    void onLeftUp(wxMouseEvent&)
    {
        _dragIndex = -1;
    }

    void onRightDown(wxMouseEvent& ev)
    {
        int index = hitTest(ev.GetPosition());

        if (index == -1) return;

        _selected = index;
        _orbitIndex = index;
        _orbitStart = ev.GetPosition();
        _orbiting = false;

        Refresh();

        _canOrbit = _owner.showSelectedInPreview();
    }

    void onRightUp(wxMouseEvent&)
    {
        if (_orbitIndex == -1) return;

        int index = _orbitIndex;
        _orbitIndex = -1;

        if (_orbiting)
        {
            _orbiting = false;
            _owner.commitThumbnailView(_tiles[index].key);
            return;
        }

        _owner.showTileMenu(_tiles[index].key);
    }

    void onMotion(wxMouseEvent& ev)
    {
        if (ev.Dragging() && ev.RightIsDown() && _orbitIndex != -1 && _canOrbit)
        {
            auto delta = ev.GetPosition() - _orbitStart;

            if (!_orbiting && std::abs(delta.x) < DRAG_THRESHOLD && std::abs(delta.y) < DRAG_THRESHOLD)
            {
                return;
            }

            _orbiting = true;
            _orbitStart = ev.GetPosition();

            _owner.orbitPreview(delta.x * ORBIT_SPEED, delta.y * ORBIT_SPEED);

            return;
        }

        if (ev.Dragging() && ev.LeftIsDown() && _dragIndex != -1)
        {
            auto delta = ev.GetPosition() - _dragStart;

            if (std::abs(delta.x) < DRAG_THRESHOLD && std::abs(delta.y) < DRAG_THRESHOLD)
            {
                return;
            }

            const auto& tile = _tiles[_dragIndex];
            _dragIndex = -1;

            wxTextDataObject data(makeAssetDragPayload(tile.type, tile.name));
            wxDropSource source(data, this);
            source.DoDragDrop(wxDrag_CopyOnly);

            return;
        }

        int index = hitTest(ev.GetPosition());

        if (index != _tooltipIndex)
        {
            _tooltipIndex = index;

            if (index != -1)
            {
                SetToolTip(_tiles[index].name);
            }
            else
            {
                UnsetToolTip();
            }
        }
    }

    ThumbnailCache& _cache;
    AssetBrowserPanel& _owner;
    std::vector<AssetTile> _tiles;
    int _thumbSize = DEFAULT_THUMBNAIL_SIZE;
    int _selected = -1;
    int _dragIndex = -1;
    int _orbitIndex = -1;
    int _tooltipIndex = -1;
    bool _orbiting = false;
    bool _canOrbit = false;
    wxPoint _dragStart;
    wxPoint _orbitStart;
};

AssetBrowserPanel::AssetBrowserPanel(wxWindow* parent) :
    DockablePanel(parent),
    _cache(new ThumbnailCache),
    _views(new ThumbnailViewStore)
{
    SetSizer(new wxBoxSizer(wxVERTICAL));

    auto* controls = new wxBoxSizer(wxHORIZONTAL);

    _modeChoice = new wxChoice(this, wxID_ANY);
    _modeChoice->Append(_("Models"));
    _modeChoice->Append(_("Entities"));
    _modeChoice->SetSelection(0);

    _filterBox = new wxTextCtrl(this, wxID_ANY);
    _filterBox->SetHint(_("Filter..."));

    _showPreview = new wxCheckBox(this, wxID_ANY, _("Show Preview"));
    _showPreview->SetValue(registry::getValue<bool>(RKEY_SHOW_PREVIEW, true));

    controls->Add(_modeChoice, 0, wxALIGN_CENTER_VERTICAL);
    controls->Add(_filterBox, 1, wxEXPAND | wxLEFT, 6);
    controls->Add(_showPreview, 0, wxALIGN_CENTER_VERTICAL | wxLEFT, 6);

    _grid = new AssetBrowserGrid(this, *_cache, *this);

    _preview = new ThumbnailPreview(this);
    _preview->getWidget()->SetMinSize(wxSize(-1, 220));

    GetSizer()->Add(controls, 0, wxEXPAND | wxALL, 6);
    GetSizer()->Add(_grid, 1, wxEXPAND | wxLEFT | wxRIGHT, 6);
    GetSizer()->Add(_preview->getWidget(), 0, wxEXPAND | wxALL, 6);

    _modeChoice->Bind(wxEVT_CHOICE, &AssetBrowserPanel::onModeChanged, this);
    _filterBox->Bind(wxEVT_TEXT, &AssetBrowserPanel::onFilterChanged, this);
    _showPreview->Bind(wxEVT_CHECKBOX, &AssetBrowserPanel::onShowPreviewToggled, this);

    applyPreviewVisibility();

    _thumbnailLoadedConn = _cache->signal_thumbnailLoaded().connect(
        [this](const std::string& key) { if (_grid->isKeyVisible(key)) _grid->Refresh(); });

    _renderRequestedConn = _cache->signal_renderRequested().connect(
        [this]() { requestIdleCallback(); });

    auto scheduleAssetReload = [this]()
    {
        GlobalUserInterface().dispatch([this]() { onAssetsChanged(); });
    };

    _vfsInitialisedConn = GlobalFileSystem().signal_Initialised().connect(scheduleAssetReload);

    _entityDefsReloadedConn = GlobalDeclarationManager()
        .signal_DeclsReloaded(decl::Type::EntityDef).connect(scheduleAssetReload);

    applyThumbnailSettings();

    _thumbnailSizeConn = GlobalRegistry().signalForKey(RKEY_THUMBNAIL_SIZE).connect(
        [this]() { onThumbnailSettingsChanged(); });

    _thumbnailPaddingConn = GlobalRegistry().signalForKey(RKEY_THUMBNAIL_PADDING).connect(
        [this]() { onThumbnailSettingsChanged(); });
}

AssetBrowserPanel::~AssetBrowserPanel()
{
    _thumbnailLoadedConn.disconnect();
    _renderRequestedConn.disconnect();
    _vfsInitialisedConn.disconnect();
    _entityDefsReloadedConn.disconnect();
    _thumbnailSizeConn.disconnect();
    _thumbnailPaddingConn.disconnect();
}

void AssetBrowserPanel::constructPreferences()
{
    if (GlobalRegistry().get(RKEY_THUMBNAIL_SIZE).empty())
    {
        registry::setValue(RKEY_THUMBNAIL_SIZE, DEFAULT_THUMBNAIL_SIZE);
    }

    if (GlobalRegistry().get(RKEY_THUMBNAIL_PADDING).empty())
    {
        registry::setValue(RKEY_THUMBNAIL_PADDING, DEFAULT_THUMBNAIL_PADDING);
    }

    auto& page = GlobalPreferenceSystem().getPage(_("Asset Browser"));

    page.appendSpinner(_("Thumbnail Size"), RKEY_THUMBNAIL_SIZE, 32, 256, 0);
    page.appendSpinner(_("Thumbnail Padding"), RKEY_THUMBNAIL_PADDING, 1, 3, 2);
}

void AssetBrowserPanel::applyThumbnailSettings()
{
    int size = registry::getValue<int>(RKEY_THUMBNAIL_SIZE, DEFAULT_THUMBNAIL_SIZE);
    size = std::max(32, std::min(256, size));

    float padding = registry::getValue<float>(RKEY_THUMBNAIL_PADDING, DEFAULT_THUMBNAIL_PADDING);

    if (padding < 1.0f)
    {
        padding = DEFAULT_THUMBNAIL_PADDING;
    }

    _thumbnailSize = size;

    _cache->setThumbnailSize(size);
    _grid->setThumbnailSize(size);
    _preview->setPadding(padding);
}

void AssetBrowserPanel::onThumbnailSettingsChanged()
{
    applyThumbnailSettings();

    _cache->clearAll();
    _grid->Refresh();
}

void AssetBrowserPanel::onPanelActivated()
{
    if (!_populated)
    {
        repopulate();
    }

    requestIdleCallback();
}

void AssetBrowserPanel::repopulate()
{
    populateAssets();
    applyFilter();

    _populated = true;
}

void AssetBrowserPanel::onAssetsChanged()
{
    if (!_populated) return;

    _assetsDirty = true;
    requestIdleCallback();
}

void AssetBrowserPanel::onIdle()
{
    if (!panelIsActive()) return;

    if (_assetsDirty)
    {
        _assetsDirty = false;
        _cache->invalidate();
        repopulate();
    }

    auto key = _cache->popNextRenderKey();

    while (!key.empty() && !_grid->isKeyVisible(key))
    {
        _cache->dropPending(key);
        key = _cache->popNextRenderKey();
    }

    if (!key.empty())
    {
        auto separator = key.find(':');
        auto type = key.substr(0, separator);
        auto name = key.substr(separator + 1);

        wxImage image;

        if (!showAssetInPreview(type, name, key))
        {
            _cache->markFailed(key);
        }
        else if (!_preview->captureImage(image, _thumbnailSize * THUMBNAIL_SUPERSAMPLE))
        {
            _cache->pushRenderKeyFront(key);
            return;
        }
        else
        {
            _cache->storeRendered(key, getCacheVariant(key), image);
        }

        if (type == assetType::Model)
        {
            GlobalModelCache().removeModel(name);
        }
    }

    if (_cache->hasPendingRenders())
    {
        requestIdleCallback();
    }
    else
    {
        showSelectedInPreview();
    }
}

void AssetBrowserPanel::populateAssets()
{
    _models.clear();
    _entityClasses.clear();

    std::set<std::string> allowedExtensions;
    string::split(allowedExtensions,
        GlobalGameManager().currentGame()->getKeyValue("modeltypes"), " ");

    game::HiddenModelFilter hiddenModels;

    GlobalFileSystem().forEachFile("models/", "*",
        [&](const vfs::FileInfo& fileInfo)
        {
            if (fileInfo.visibility != vfs::Visibility::NORMAL) return;
            if (hiddenModels.isHidden(fileInfo.name)) return;

            auto extension = string::to_lower_copy(os::getExtension(fileInfo.name));

            if (allowedExtensions.count(extension) > 0)
            {
                _models.push_back(fileInfo.fullPath());
            }
        }, 0);

    GlobalEntityClassManager().forEachEntityClass(
        [this](const scene::EntityClass::Ptr& eclass)
        {
            if (eclass->getVisibility() == vfs::Visibility::HIDDEN) return;
            if (!eclass->isFixedSize()) return;

            _entityClasses.push_back(eclass->getDeclName());
        });

    std::sort(_models.begin(), _models.end());
    std::sort(_entityClasses.begin(), _entityClasses.end());
}

void AssetBrowserPanel::applyFilter()
{
    bool models = _modeChoice->GetSelection() == 0;
    auto filter = string::to_lower_copy(_filterBox->GetValue().ToStdString());

    auto matchesFilter = [&](const std::string& name)
    {
        if (filter.empty()) return true;

        return std::search(name.begin(), name.end(), filter.begin(), filter.end(),
            [](char nameChar, char filterChar)
            {
                return std::tolower(static_cast<unsigned char>(nameChar)) == filterChar;
            }) != name.end();
    };

    std::vector<AssetTile> tiles;

    for (const auto& name : models ? _models : _entityClasses)
    {
        if (!matchesFilter(name))
        {
            continue;
        }

        AssetTile tile;
        tile.type = models ? assetType::Model : assetType::EntityClass;
        tile.name = name;
        tile.label = os::getFilename(name);
        tile.key = tile.type + ":" + name;

        tiles.push_back(std::move(tile));
    }

    _grid->setTiles(std::move(tiles));
}

bool AssetBrowserPanel::showSelectedInPreview()
{
    if (!_showPreview->GetValue()) return false;

    if (_cache->hasPendingRenders()) return false;

    const auto* tile = _grid->getSelectedTile();

    if (tile == nullptr) return false;

    return showAssetInPreview(tile->type, tile->name, tile->key);
}

bool AssetBrowserPanel::showAssetInPreview(const std::string& type, const std::string& name,
    const std::string& key)
{
    _previewKey.clear();

    if (!_preview->showAsset(type, name)) return false;

    const auto* angles = _views->find(key);

    if (angles != nullptr)
    {
        _preview->setAssetViewAngles(*angles);
    }

    _previewKey = key;

    return true;
}

std::string AssetBrowserPanel::getCacheVariant(const std::string& key) const
{
    const auto* angles = _views->find(key);

    if (angles == nullptr) return {};

    return string::to_string((*angles)[camera::CAMERA_PITCH]) + "|"
        + string::to_string((*angles)[camera::CAMERA_YAW]);
}

void AssetBrowserPanel::orbitPreview(double deltaYaw, double deltaPitch)
{
    auto angles = _preview->getAssetViewAngles();

    angles[camera::CAMERA_YAW] += deltaYaw;
    angles[camera::CAMERA_PITCH] = std::max(-89.0,
        std::min(89.0, angles[camera::CAMERA_PITCH] + deltaPitch));

    _preview->setAssetViewAngles(angles);
}

void AssetBrowserPanel::commitThumbnailView(const std::string& key)
{
    if (_previewKey != key) return;

    _views->set(key, _preview->getAssetViewAngles());
    _cache->rerender(key);

    requestIdleCallback();
}

void AssetBrowserPanel::showTileMenu(const std::string& key)
{
    wxMenu menu;

    if (_previewKey == key)
    {
        menu.Append(MENU_USE_PREVIEW_ANGLE, _("Use Preview Angle"));
    }

    menu.Append(MENU_RESET_VIEW, _("Reset Thumbnail View"));
    menu.AppendSeparator();
    menu.Append(MENU_REBUILD, _("Rebuild Thumbnail"));
    menu.Append(MENU_REBUILD_ALL, _("Rebuild All Thumbnails"));

    menu.Bind(wxEVT_MENU, [this, key](wxCommandEvent& ev) { onTileMenuItem(key, ev.GetId()); });

    _grid->PopupMenu(&menu);
}

void AssetBrowserPanel::onTileMenuItem(const std::string& key, int id)
{
    switch (id)
    {
    case MENU_USE_PREVIEW_ANGLE:
        commitThumbnailView(key);
        break;

    case MENU_RESET_VIEW:
        _views->remove(key);
        _cache->rerender(key);
        requestIdleCallback();
        break;

    case MENU_REBUILD:
        _cache->rerender(key);
        requestIdleCallback();
        break;

    case MENU_REBUILD_ALL:
        _cache->clearAll();
        _grid->Refresh();
        break;
    }
}

void AssetBrowserPanel::applyPreviewVisibility()
{
    bool visible = _showPreview->GetValue();

    GetSizer()->Show(_preview->getWidget(), visible);
    GetSizer()->Layout();

    if (visible)
    {
        showSelectedInPreview();
    }
}

void AssetBrowserPanel::onShowPreviewToggled(wxCommandEvent&)
{
    registry::setValue(RKEY_SHOW_PREVIEW, _showPreview->GetValue());

    applyPreviewVisibility();
}

void AssetBrowserPanel::onModeChanged(wxCommandEvent&)
{
    applyFilter();
}

void AssetBrowserPanel::onFilterChanged(wxCommandEvent&)
{
    applyFilter();
}

}
