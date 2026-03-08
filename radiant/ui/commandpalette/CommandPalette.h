#pragma once

#include "icommandsystem.h"
#include "CommandMetadata.h"
#include "wxutil/dialog/DialogBase.h"
#include <wx/textctrl.h>
#include <wx/vlbox.h>
#include <string>
#include <vector>

namespace ui
{

class CommandPaletteListBox;

class CommandPalette : public wxutil::DialogBase
{
public:
	static void ShowPalette(const cmd::ArgumentList& args);

private:
	friend class CommandPaletteListBox;

	struct CommandEntry
	{
		std::string internalName;
		std::string displayName;
		std::string description;
		std::string shortcut;
		CmdCategory category;
		bool canExecute;
		int relevance = 0;
	};

	wxTextCtrl* _searchBox;
	CommandPaletteListBox* _list;

	std::vector<CommandEntry> _allCommands;
	std::vector<int> _filtered; // indices into _allCommands

	CommandPalette(wxWindow* parent);

	void populateCommands();
	void applyFilter(const std::string& text);
	void updateList();
	void executeSelected();

	static bool fuzzyMatch(const std::string& text, const std::string& pattern);

	void onSearchChanged(wxCommandEvent& ev);
	void onKeyDown(wxKeyEvent& ev);
	void onDeactivate(wxActivateEvent& ev);
};

class CommandPaletteListBox : public wxVListBox
{
public:
	CommandPaletteListBox(CommandPalette* parent);

private:
	CommandPalette* _palette;

	void OnDrawItem(wxDC& dc, const wxRect& rect, size_t n) const override;
	wxCoord OnMeasureItem(size_t n) const override;
	void OnDrawBackground(wxDC& dc, const wxRect& rect, size_t n) const override;
};

} // namespace ui
