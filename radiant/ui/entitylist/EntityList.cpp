#include "EntityList.h"

#include "icommandsystem.h"

#include "registry/Widgets.h"
#include "entitylib.h"
#include "scenelib.h"
#include "inode.h"
#include "iselectable.h"
#include "i18n.h"

#include "wxutil/dataview/TreeView.h"

#include <wx/sizer.h>
#include <wx/checkbox.h>
#include <wx/textctrl.h>
#include <wx/wupdlock.h>

#include "util/ScopedBoolLock.h"

namespace ui
{

namespace
{
	const std::string RKEY_ROOT = "user/ui/entityList/";
	const std::string RKEY_ENTITYLIST_FOCUS_SELECTION = RKEY_ROOT + "focusSelection";
	const std::string RKEY_ENTITYLIST_VISIBLE_ONLY = RKEY_ROOT + "visibleNodesOnly";

	const int FILTER_DEBOUNCE_MSECS = 100;
}

EntityList::EntityList(wxWindow* parent) :
    DockablePanel(parent),
	_callbackActive(false),
	_refreshTreeModelOnIdle(false),
	_filterDebounceTimer(this)
{
	populateWindow();
}
    
EntityList::~EntityList()
{
    _filterDebounceTimer.Stop();

    _nodesToUpdate.clear();

    // In OSX we might receive callbacks during shutdown, so disable any events
    if (_treeView != nullptr)
    {
        _treeView->Unbind(wxEVT_DATAVIEW_SELECTION_CHANGED, &EntityList::onSelection, this);
        _treeView->Unbind(wxEVT_DATAVIEW_ITEM_EXPANDED, &EntityList::onRowExpand, this);
        _treeView->Unbind(wxEVT_CHAR, &EntityList::onTreeViewChar, this);
    }

    if (panelIsActive())
    {
        disconnectListeners();
    }
}

void EntityList::onPanelActivated()
{
    connectListeners();

    // Avoid repopulatig inside the event handler
    scheduleTreeModelRefresh();
}

void EntityList::onPanelDeactivated()
{
    disconnectListeners();

    _filterDebounceTimer.Stop();

    // Unselect everything when hiding the dialog
    util::ScopedBoolLock lock(_callbackActive);
    _treeView->UnselectAll();

    cancelCallbacks();
    _refreshTreeModelOnIdle = false;
    _itemToScrollToWhenIdle.Unset();
    _nodesToUpdate.clear();
}

void EntityList::connectListeners()
{
    // Observe the scenegraph
    _treeModel.connectToSceneGraph();

    // Register self to the SelSystem to get notified upon selection changes.
    GlobalSelectionSystem().addObserver(this);

    // Get notified when filters are changing
    _filtersConfigChangedConn = GlobalFilterSystem().filterConfigChangedSignal().connect(
        sigc::mem_fun(*this, &EntityList::onFilterConfigChanged)
    );
}

void EntityList::disconnectListeners()
{
    _treeModel.disconnectFromSceneGraph();

    // Disconnect from the filters-changed signal
    _filtersConfigChangedConn.disconnect();

    // De-register self from the SelectionSystem
    GlobalSelectionSystem().removeObserver(this);
}

void EntityList::populateWindow()
{
	SetSizer(new wxBoxSizer(wxVERTICAL));

	wxBoxSizer* vbox = new wxBoxSizer(wxVERTICAL);
	GetSizer()->Add(vbox, 1, wxEXPAND | wxALL, 12);

	// Configure the treeview
	_treeView = wxutil::TreeView::CreateWithModel(
        this, _treeModel.getModel().get(), 
#if defined(__linux__)
		wxDV_MULTIPLE
#else
		wxDV_NO_HEADER | wxDV_MULTIPLE
#endif
    );

	// Single column with icon and name
	_treeView->AppendTextColumn(_("Name"), _treeModel.getColumns().name.getColumnIndex(),
		wxDATAVIEW_CELL_INERT, wxCOL_WIDTH_AUTOSIZE, wxALIGN_NOT, wxDATAVIEW_COL_SORTABLE);

	_treeView->EnableSearchPopup(false);
	_filterColumns.push_back(_treeModel.getColumns().name);
	setupTreeModelFilter();

	_treeView->Bind(wxEVT_DATAVIEW_SELECTION_CHANGED, &EntityList::onSelection, this);
	_treeView->Bind(wxEVT_DATAVIEW_ITEM_EXPANDED, &EntityList::onRowExpand, this);

	_filterBox = new wxTextCtrl(this, wxID_ANY);
	_filterBox->SetHint(_("Type here to filter"));
	_filterBox->Bind(wxEVT_TEXT, &EntityList::onFilterTextChanged, this);
	_filterBox->Bind(wxEVT_CHAR_HOOK, &EntityList::onFilterBoxKey, this);
	_treeView->Bind(wxEVT_CHAR, &EntityList::onTreeViewChar, this);

	Bind(wxEVT_TIMER, &EntityList::onFilterDebounceTimer, this);

	// Update the toggle item status according to the registry

	_focusSelected = new wxCheckBox(this, wxID_ANY, _("Focus camera on selected entity"));
	_visibleOnly = new wxCheckBox(this, wxID_ANY, _("List visible nodes only"));

	registry::bindWidget(_focusSelected, RKEY_ENTITYLIST_FOCUS_SELECTION);
    registry::bindWidget(_visibleOnly, RKEY_ENTITYLIST_VISIBLE_ONLY);

	vbox->Add(_treeView, 1, wxEXPAND | wxBOTTOM, 6);
	vbox->Add(_filterBox, 0, wxEXPAND | wxBOTTOM, 6);
	vbox->Add(_focusSelected, 0, wxBOTTOM, 6);
	vbox->Add(_visibleOnly, 0);

	_treeModel.setConsiderVisibleNodesOnly(_visibleOnly->GetValue());

	// Connect the toggle buttons' "toggled" signal
	_visibleOnly->Bind(wxEVT_CHECKBOX, &EntityList::onVisibleOnlyToggle, this);
}

void EntityList::updateSelectionStatus()
{
	// Disable callbacks and traverse the treemodel
    util::ScopedBoolLock lock(_callbackActive);

    wxWindowUpdateLocker freezer(_treeView);

	// Traverse the entire tree, updating the selection
	_treeModel.updateSelectionStatus(std::bind(&EntityList::onTreeViewSelection, this,
		std::placeholders::_1, std::placeholders::_2));
}

void EntityList::scheduleTreeModelRefresh()
{
    _refreshTreeModelOnIdle = true;
    requestIdleCallback();
}

void EntityList::refreshTreeModel()
{
    // Refresh the whole tree
    _nodesToUpdate.clear();
    _itemToScrollToWhenIdle.Unset();

    _treeModel.refresh();

    // If the underlying model was replaced, rewrap it in a fresh filter
    if (!_treeModelFilter || _treeModelFilter->GetChildModel().get() != _treeModel.getModel().get())
    {
        setupTreeModelFilter();
    }

    expandRootNode();

    // Update the selection status of all nodes
    updateSelectionStatus();
}

void EntityList::selectionChanged(const scene::INodePtr& node, bool isComponent)
{
    // Ignore all types except entities, also ignore components
    // Don't update during scene updates caused by ourselves
    if (_callbackActive || isComponent || node->getNodeType() != scene::INode::Type::Entity) return;

    util::ScopedBoolLock lock(_callbackActive);
    wxWindowUpdateLocker freezer(_treeView);

    // Remember this for later, we select the tree items during idle processing
    _nodesToUpdate.push_back(node);
    requestIdleCallback();
}

void EntityList::onFilterConfigChanged()
{
    // Only react to filter changes if we display visible nodes only otherwise
    // we don't care
	if (_visibleOnly->GetValue())
	{
		// When filters are changed possibly any node could have changed 
        // its visibility, so refresh the whole tree
        scheduleTreeModelRefresh();
	}
}

void EntityList::onRowExpand(wxDataViewEvent& ev)
{
	if (_callbackActive) return; // avoid loops

	// greebo: This is a possible optimisation point. Don't update the entire tree,
	// but only the expanded subtree.
    updateSelectionStatus();
}

void EntityList::onVisibleOnlyToggle(wxCommandEvent& ev)
{
    _treeModel.setConsiderVisibleNodesOnly(_visibleOnly->GetValue());

	// Update the whole tree
	scheduleTreeModelRefresh();
}

void EntityList::setupTreeModelFilter()
{
	_treeModelFilter.reset(new wxutil::TreeModelFilter(_treeModel.getModel()));
	_treeModelFilter->SetVisibleFunc(
		std::bind(&EntityList::treeModelRowIsVisible, this, std::placeholders::_1));

	_treeView->AssociateModel(_treeModelFilter.get());
}

bool EntityList::treeModelRowIsVisible(wxutil::TreeModel::Row& row)
{
	if (_filterText.empty()) return true;

	if (wxutil::TreeModel::RowContainsString(row, _filterText, _filterColumns, true))
	{
		return true;
	}

	wxDataViewItemArray children;
	_treeModel.getModel()->GetChildren(row.getItem(), children);

	for (const auto& child : children)
	{
		wxutil::TreeModel::Row childRow(child, *_treeModel.getModel());

		if (treeModelRowIsVisible(childRow)) return true;
	}

	return false;
}

void EntityList::onFilterTextChanged(wxCommandEvent& ev)
{
	_filterDebounceTimer.StartOnce(FILTER_DEBOUNCE_MSECS);
}

void EntityList::onFilterDebounceTimer(wxTimerEvent& ev)
{
	applyFilter();
}

void EntityList::onTreeViewChar(wxKeyEvent& ev)
{
	wxChar uc = ev.GetUnicodeKey();

	if (uc != WXK_NONE && uc >= 32)
	{
		_filterBox->SetFocus();
		_filterBox->SetInsertionPointEnd();
		_filterBox->WriteText(wxString(uc));
		return;
	}

	ev.Skip();
}

void EntityList::onFilterBoxKey(wxKeyEvent& ev)
{
	if (ev.GetKeyCode() == WXK_ESCAPE && !_filterBox->GetValue().empty())
	{
		_filterDebounceTimer.Stop();
		_filterBox->ChangeValue(wxEmptyString);
		applyFilter();
		_treeView->SetFocus();
		return;
	}

	ev.Skip();
}

void EntityList::applyFilter()
{
	_filterText = _filterBox->GetValue().Lower();

	if (!_treeModelFilter) return;

	_treeModelFilter->Cleared();

	expandRootNode();
	updateSelectionStatus();
}

void EntityList::expandRootNode()
{
	auto rootNode = _treeModel.find(GlobalSceneGraph().root());

	if (rootNode && !_treeView->IsExpanded(rootNode->getIter()))
	{
		_treeView->Expand(rootNode->getIter());
	}
}

void EntityList::onIdle()
{
    if (_refreshTreeModelOnIdle)
    {
        if (wxWindow::GetCapture() != nullptr)
        {
            requestIdleCallback();
            return;
        }

        _refreshTreeModelOnIdle = false;

        util::ScopedBoolLock lock(_callbackActive);
        refreshTreeModel();
        return;
    }

    if (!_nodesToUpdate.empty())
    {
        for (const auto& weakNode : _nodesToUpdate)
        {
            auto node = weakNode.lock();
            if (!node) continue;

            _treeModel.updateSelectionStatus(node, std::bind(&EntityList::onTreeViewSelection, this,
                std::placeholders::_1, std::placeholders::_2));
        }

        _nodesToUpdate.clear();
    }

    if (_itemToScrollToWhenIdle.IsOk())
    {
        _treeView->EnsureVisible(_itemToScrollToWhenIdle);
        _itemToScrollToWhenIdle.Unset();
    }
}

void EntityList::onTreeViewSelection(const wxDataViewItem& item, bool selected)
{
	if (selected)
	{
		// Select the row in the TreeView
		_treeView->Select(item);

		// Scroll to the row, but don't do this immediately (can be expensive)
        _itemToScrollToWhenIdle = item;
        requestIdleCallback();
	}
	else
	{
		_treeView->Unselect(item);
	} 
}

void EntityList::onSelection(wxDataViewEvent& ev)
{
	if (_callbackActive) return; // avoid loops

	auto view = static_cast<wxutil::TreeView*>(ev.GetEventObject());

	wxDataViewItemArray newSelection;
	view->GetSelections(newSelection);

    std::set<scene::INode*> desiredSelection;
    std::set<scene::INode*> mapSelection;

    for (const auto& item : newSelection)
    {
        // Load the instance pointer from the columns
        wxutil::TreeModel::Row row(item, *_treeModel.getModel());
        desiredSelection.insert(static_cast<scene::INode*>(row[_treeModel.getColumns().node].getPointer()));
    }

    // Check the existing map selection to run a diff
    GlobalSelectionSystem().foreachSelected([&](const scene::INodePtr& node)
    {
        mapSelection.insert(node.get());
    });

	// Calculate the difference between the new selection and the old one
    std::vector<scene::INode*> diff;
    diff.reserve(desiredSelection.size() + mapSelection.size());

	std::set_symmetric_difference(desiredSelection.begin(), desiredSelection.end(),
        mapSelection.begin(), mapSelection.end(), std::back_inserter(diff));

	for (auto node : diff)
	{
		auto selectable = dynamic_cast<ISelectable*>(node);

        if (selectable == nullptr) continue;

        // Disable update to avoid loopbacks
        _callbackActive = true;

        // Should the instance be selected?
        bool shouldBeSelected = desiredSelection.count(node) > 0;
        selectable->setSelected(shouldBeSelected);

        if (shouldBeSelected && _focusSelected->GetValue())
        {
            auto originAndAngles = scene::getOriginAndAnglesToLookAtNode(*node);
            GlobalCommandSystem().executeCommand("FocusViews", cmd::ArgumentList{ originAndAngles.first, originAndAngles.second });
        }

            // Now reactivate the callbacks
        _callbackActive = false;
	}
}

} // namespace ui
