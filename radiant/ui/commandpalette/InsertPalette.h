#pragma once

#include "icommandsystem.h"
#include "wxutil/dialog/DialogBase.h"
#include <wx/textctrl.h>
#include <wx/vlbox.h>
#include <string>
#include <vector>

namespace ui
{

class InsertPaletteListBox;

class InsertPalette : public wxutil::DialogBase
{
public:
	static void ShowPalette(const cmd::ArgumentList& args);

private:
	friend class InsertPaletteListBox;

	enum class AssetType
	{
		EntityClass,
		Model,
		Prefab,
		Particle,
	};

	struct AssetEntry
	{
		std::string name;
		std::string description;
		AssetType type;
	};

	wxTextCtrl* _searchBox;
	InsertPaletteListBox* _list;

	std::vector<AssetEntry> _allAssets;
	std::vector<int> _filtered;

	InsertPalette(wxWindow* parent);

	void populateAssets();
	void applyFilter(const std::string& text);
	void updateList();
	void insertSelected();

	static bool fuzzyMatch(const std::string& text, const std::string& pattern);
	static const char* typeLabel(AssetType type);

	void onSearchChanged(wxCommandEvent& ev);
	void onKeyDown(wxKeyEvent& ev);
	void onDeactivate(wxActivateEvent& ev);
};

class InsertPaletteListBox : public wxVListBox
{
public:
	InsertPaletteListBox(InsertPalette* parent);

private:
	InsertPalette* _palette;

	void OnDrawItem(wxDC& dc, const wxRect& rect, size_t n) const override;
	wxCoord OnMeasureItem(size_t n) const override;
	void OnDrawBackground(wxDC& dc, const wxRect& rect, size_t n) const override;
};

} // namespace ui
