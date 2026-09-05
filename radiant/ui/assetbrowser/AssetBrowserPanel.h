#pragma once

#include <memory>
#include <string>
#include <vector>

#include <sigc++/connection.h>

#include "wxutil/DockablePanel.h"
#include "wxutil/event/SingleIdleCallback.h"

class wxCheckBox;
class wxChoice;
class wxCommandEvent;
class wxTextCtrl;

namespace ui
{

class AssetBrowserGrid;
class ThumbnailCache;
class ThumbnailPreview;
class ThumbnailViewStore;

class AssetBrowserPanel :
    public wxutil::DockablePanel,
    public wxutil::SingleIdleCallback
{
    friend class AssetBrowserGrid;

public:
    AssetBrowserPanel(wxWindow* parent);
    ~AssetBrowserPanel() override;

    static void constructPreferences();

protected:
    void onPanelActivated() override;
    void onIdle() override;

private:
    void populateAssets();
    void repopulate();
    void onAssetsChanged();
    void applyThumbnailSettings();
    void onThumbnailSettingsChanged();
    void applyFilter();
    bool showSelectedInPreview();
    bool showAssetInPreview(const std::string& type, const std::string& name, const std::string& key);
    std::string getCacheVariant(const std::string& key) const;
    void orbitPreview(double deltaYaw, double deltaPitch);
    void commitThumbnailView(const std::string& key);
    void showTileMenu(const std::string& key);
    void onTileMenuItem(const std::string& key, int id);
    void applyPreviewVisibility();
    void onModeChanged(wxCommandEvent& ev);
    void onFilterChanged(wxCommandEvent& ev);
    void onShowPreviewToggled(wxCommandEvent& ev);

    wxChoice* _modeChoice;
    wxTextCtrl* _filterBox;
    wxCheckBox* _showPreview;
    AssetBrowserGrid* _grid;
    ThumbnailPreview* _preview;
    std::unique_ptr<ThumbnailCache> _cache;
    std::unique_ptr<ThumbnailViewStore> _views;

    std::vector<std::string> _models;
    std::vector<std::string> _entityClasses;
    std::string _previewKey;
    int _thumbnailSize = 0;
    bool _populated = false;
    bool _assetsDirty = false;

    sigc::connection _thumbnailLoadedConn;
    sigc::connection _renderRequestedConn;
    sigc::connection _vfsInitialisedConn;
    sigc::connection _entityDefsReloadedConn;
    sigc::connection _thumbnailSizeConn;
    sigc::connection _thumbnailPaddingConn;
};

}
