#include "GuiChooser.h"

#include "i18n.h"
#include "ifilesystem.h"
#include "igui.h"

#include "wxutil/Bitmap.h"

#include <wx/sizer.h>
#include <wx/button.h>
#include <wx/splitter.h>

namespace ui
{

namespace
{
	const char* const WINDOW_TITLE = N_("Choose GUI Definition");

	const int WINDOW_WIDTH = 900;
	const int WINDOW_HEIGHT = 650;

	const char* const GUI_ICON = "sr_icon_readable.png";
	const char* const FOLDER_ICON = "folder16.png";

	const std::string GUI_DIR = "guis/";
	const std::string GUI_EXT = "gui";
}

GuiChooser::GuiChooser(const std::string& initialSelection, wxWindow* parent) :
	DialogBase(_(WINDOW_TITLE), parent, "GuiChooser"),
	_store(new wxutil::TreeModel(_columns)),
	_treeView(nullptr),
	_preview(nullptr),
	_panedPosition("guiChooserSplitter"),
	_guiIcon(wxutil::GetLocalBitmap(GUI_ICON)),
	_folderIcon(wxutil::GetLocalBitmap(FOLDER_ICON)),
	_initialSelection(initialSelection)
{
	SetSize(WINDOW_WIDTH, WINDOW_HEIGHT);

	populateWindow();
	fillTree();

	FindWindowById(wxID_OK, this)->Enable(false);

	RegisterPersistableObject(&_panedPosition);
}

void GuiChooser::populateWindow()
{
	SetSizer(new wxBoxSizer(wxVERTICAL));

	wxBoxSizer* vbox = new wxBoxSizer(wxVERTICAL);
	GetSizer()->Add(vbox, 1, wxEXPAND | wxALL, 12);

	auto splitter = new wxSplitterWindow(this, wxID_ANY,
		wxDefaultPosition, wxDefaultSize, wxSP_3D | wxSP_LIVE_UPDATE);
	splitter->SetMinimumPaneSize(10);

	_treeView = wxutil::TreeView::CreateWithModel(splitter, _store.get(), wxDV_NO_HEADER);
	_treeView->AppendIconTextColumn(_("GUI Path"), _columns.name.getColumnIndex(),
		wxDATAVIEW_CELL_INERT, wxCOL_WIDTH_AUTOSIZE, wxALIGN_NOT, wxDATAVIEW_COL_SORTABLE);

	_treeView->AddSearchColumn(_columns.name);

	_treeView->Bind(wxEVT_DATAVIEW_SELECTION_CHANGED,
		&GuiChooser::onSelectionChanged, this);
	_treeView->Bind(wxEVT_DATAVIEW_ITEM_ACTIVATED,
		&GuiChooser::onItemActivated, this);

	_preview = new wxutil::GuiView(splitter);

	splitter->SplitVertically(_treeView, _preview,
		static_cast<int>(WINDOW_WIDTH * 0.35f));

	_panedPosition.connect(splitter);

	vbox->Add(splitter, 1, wxEXPAND | wxBOTTOM, 6);
	vbox->Add(CreateStdDialogButtonSizer(wxOK | wxCANCEL), 0, wxALIGN_RIGHT);
}

void GuiChooser::fillTree()
{
	wxutil::VFSTreePopulator populator(_store);

	GlobalFileSystem().forEachFile(GUI_DIR, GUI_EXT,
		[&](const vfs::FileInfo& fileInfo)
		{
			populator.addPath(fileInfo.fullPath());
		},
		99
	);

	populator.forEachNode(*this);

	_store->SortModelFoldersFirst(_columns.name, _columns.isFolder);
}

void GuiChooser::visit(wxutil::TreeModel& /* store */, wxutil::TreeModel::Row& row,
	const std::string& path, bool isExplicit)
{
	std::string displayName = path.substr(path.rfind("/") + 1);

	if (isExplicit)
	{
		auto dotPos = displayName.rfind('.');
		if (dotPos != std::string::npos)
		{
			displayName = displayName.substr(0, dotPos);
		}
	}

	row[_columns.name] = wxVariant(wxDataViewIconText(displayName, isExplicit ? _guiIcon : _folderIcon));
	row[_columns.fullName] = path;
	row[_columns.isFolder] = !isExplicit;

	row.SendItemAdded();
}

void GuiChooser::selectInitialItem()
{
	if (_initialSelection.empty()) return;

	auto item = _store->FindString(_initialSelection, _columns.fullName);

	if (item.IsOk())
	{
		_treeView->Select(item);
		_treeView->EnsureVisible(item);
	}
}

int GuiChooser::ShowModal()
{
	selectInitialItem();
	return DialogBase::ShowModal();
}

void GuiChooser::updatePreview()
{
	gui::IGuiPtr gui;

	if (!_selection.empty())
	{
		try
		{
			gui = GlobalGuiManager().getGui(_selection);
		}
		catch (const std::exception&)
		{
			gui.reset();
		}
	}

	_preview->setGui(gui);

	if (gui)
	{
		gui->initTime(0);
	}

	_preview->redraw();
}

void GuiChooser::onSelectionChanged(wxDataViewEvent&)
{
	auto item = _treeView->GetSelection();
	auto okButton = FindWindowById(wxID_OK, this);

	if (!item.IsOk())
	{
		_selection.clear();
		okButton->Enable(false);
		updatePreview();
		return;
	}

	wxutil::TreeModel::Row row(item, *_treeView->GetModel());

	if (row[_columns.isFolder].getBool())
	{
		_selection.clear();
		okButton->Enable(false);
		updatePreview();
		return;
	}

	_selection = row[_columns.fullName];
	okButton->Enable(true);
	updatePreview();
}

void GuiChooser::onItemActivated(wxDataViewEvent&)
{
	if (!_selection.empty())
	{
		EndModal(wxID_OK);
	}
}

std::string GuiChooser::ChooseGui(const std::string& prevSelection, wxWindow* parent)
{
	auto* dialog = new GuiChooser(prevSelection, parent);

	std::string result = prevSelection;

	if (dialog->ShowModal() == wxID_OK)
	{
		result = dialog->_selection;
	}

	dialog->Destroy();

	return result;
}

} // namespace
