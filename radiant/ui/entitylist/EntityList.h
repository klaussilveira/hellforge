#pragma once

#include "iselection.h"
#include "GraphTreeModel.h"
#include <sigc++/connection.h>

#include "wxutil/DockablePanel.h"
#include "wxutil/dataview/TreeModelFilter.h"
#include "wxutil/event/SingleIdleCallback.h"

#include <wx/timer.h>

namespace wxutil
{
	class TreeView;
}

class wxCheckBox;
class wxTextCtrl;

namespace ui
{

class EntityList :
	public wxutil::DockablePanel,
    public selection::SelectionSystem::Observer,
    public wxutil::SingleIdleCallback
{
private:
	// The GraphTreeModel instance
	GraphTreeModel _treeModel;

	bool _callbackActive;
	bool _refreshTreeModelOnIdle;

	wxutil::TreeView* _treeView;

	wxutil::TreeModelFilter::Ptr _treeModelFilter;
	std::vector<wxutil::TreeModel::Column> _filterColumns;
	wxString _filterText;

	wxTextCtrl* _filterBox;
	wxTimer _filterDebounceTimer;

	wxCheckBox* _focusSelected;
	wxCheckBox* _visibleOnly;

	sigc::connection _filtersConfigChangedConn;

    wxDataViewItem _itemToScrollToWhenIdle;
    std::vector<scene::INodeWeakPtr> _nodesToUpdate;

public:
	EntityList(wxWindow* parent);
    ~EntityList() override;

protected:
    void onPanelActivated() override;
    void onPanelDeactivated() override;

    void onIdle() override;

private:
    void connectListeners();
    void disconnectListeners();

	/** greebo: Creates the widgets
	 */
	void populateWindow();

	/** greebo: Updates the treeview contents
	 */
	void updateSelectionStatus();

    // Request a full treestore refresh during the next idle period
    void scheduleTreeModelRefresh();

    // Repopulate the entire treestore from the scenegraph
    void refreshTreeModel();

	/** 
	 * greebo: SelectionSystem::Observer implementation.
	 * Gets notified as soon as the selection is changed.
	 */
	void selectionChanged(const scene::INodePtr& node, bool isComponent) override;

	// Called by the graph tree model
	void onTreeViewSelection(const wxDataViewItem& item, bool selected);

	void onFilterConfigChanged();

	void onRowExpand(wxDataViewEvent& ev);

	// Called when the user is updating the treeview selection
	void onSelection(wxDataViewEvent& ev);
	void onVisibleOnlyToggle(wxCommandEvent& ev);
	void onFilterTextChanged(wxCommandEvent& ev);
	void onFilterDebounceTimer(wxTimerEvent& ev);
	void onFilterBoxKey(wxKeyEvent& ev);
	void onTreeViewChar(wxKeyEvent& ev);

	void setupTreeModelFilter();
	void applyFilter();
	bool treeModelRowIsVisible(wxutil::TreeModel::Row& row);

	void expandRootNode();
};

} // namespace
