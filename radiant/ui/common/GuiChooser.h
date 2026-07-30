#pragma once

#include <string>

#include "wxutil/dialog/DialogBase.h"
#include "wxutil/dataview/TreeModel.h"
#include "wxutil/dataview/TreeView.h"
#include "wxutil/dataview/VFSTreePopulator.h"
#include "wxutil/preview/GuiView.h"
#include "wxutil/PanedPosition.h"
#include "wxutil/Icon.h"

namespace ui
{

class GuiChooser :
	public wxutil::DialogBase,
	public wxutil::VFSTreePopulator::Visitor
{
public:
	struct Columns :
		public wxutil::TreeModel::ColumnRecord
	{
		Columns() :
			name(add(wxutil::TreeModel::Column::IconText)),
			fullName(add(wxutil::TreeModel::Column::String)),
			isFolder(add(wxutil::TreeModel::Column::Boolean))
		{}

		wxutil::TreeModel::Column name;
		wxutil::TreeModel::Column fullName;
		wxutil::TreeModel::Column isFolder;
	};

private:
	Columns _columns;
	wxutil::TreeModel::Ptr _store;
	wxutil::TreeView* _treeView;
	wxutil::GuiView* _preview;
	wxutil::PanedPosition _panedPosition;

	wxutil::Icon _guiIcon;
	wxutil::Icon _folderIcon;

	std::string _selection;
	std::string _initialSelection;

public:
	static std::string ChooseGui(const std::string& prevSelection, wxWindow* parent = nullptr);

	void visit(wxutil::TreeModel& store, wxutil::TreeModel::Row& row,
		const std::string& path, bool isExplicit) override;

	int ShowModal() override;

private:
	GuiChooser(const std::string& initialSelection, wxWindow* parent);

	void populateWindow();
	void fillTree();
	void selectInitialItem();
	void updatePreview();

	void onSelectionChanged(wxDataViewEvent& ev);
	void onItemActivated(wxDataViewEvent& ev);
};

} // namespace
