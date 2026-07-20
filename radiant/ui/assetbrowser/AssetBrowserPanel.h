#pragma once

#include <memory>
#include <string>
#include <vector>

#include <sigc++/connection.h>

#include "wxutil/DockablePanel.h"
#include "wxutil/event/SingleIdleCallback.h"

class wxChoice;
class wxCommandEvent;
class wxTextCtrl;

namespace ui
{

class AssetBrowserGrid;
class ThumbnailCache;
class ThumbnailPreview;

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
    void showSelectedInPreview();
    void onModeChanged(wxCommandEvent& ev);
    void onFilterChanged(wxCommandEvent& ev);

    wxChoice* _modeChoice;
    wxTextCtrl* _filterBox;
    AssetBrowserGrid* _grid;
    ThumbnailPreview* _preview;
    std::unique_ptr<ThumbnailCache> _cache;

    std::vector<std::string> _models;
    std::vector<std::string> _entityClasses;
    bool _populated = false;
    bool _assetsDirty = false;

    sigc::connection _thumbnailLoadedConn;
    sigc::connection _renderRequestedConn;
    sigc::connection _vfsInitialisedConn;
    sigc::connection _entityDefsReloadedConn;
    sigc::connection _thumbnailSizeConn;
    sigc::connection _thumbnailZoomConn;
};

}
