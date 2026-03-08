#include "CommandPalette.h"

#include "i18n.h"
#include "icommandsystem.h"
#include "iselection.h"
#include "selectionlib.h"
#include "ui/ieventmanager.h"
#include "ui/imainframe.h"

#include <wx/sizer.h>
#include <wx/settings.h>
#include <wx/dcmemory.h>

#include <algorithm>
#include <cctype>

namespace ui
{

namespace
{

std::string camelCaseToWords(const std::string& name)
{
	std::string result;
	for (size_t i = 0; i < name.size(); ++i)
	{
		if (i > 0 && std::isupper(static_cast<unsigned char>(name[i])) &&
			!std::isupper(static_cast<unsigned char>(name[i - 1])))
		{
			result += ' ';
		}
		result += name[i];
	}
	return result;
}

constexpr int ROW_PADDING = 6;
constexpr int TEXT_LEFT_MARGIN = 8;
constexpr int TEXT_RIGHT_MARGIN = 8;
constexpr int SHORTCUT_RIGHT_MARGIN = 12;
constexpr int DESC_TOP_GAP = 1;

} // namespace

CommandPaletteListBox::CommandPaletteListBox(CommandPalette* parent) :
	wxVListBox(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxBORDER_NONE),
	_palette(parent)
{
}

void CommandPaletteListBox::OnDrawBackground(wxDC& dc, const wxRect& rect, size_t n) const
{
	if (IsSelected(n))
	{
		dc.SetBrush(wxBrush(wxSystemSettings::GetColour(wxSYS_COLOUR_HIGHLIGHT)));
		dc.SetPen(*wxTRANSPARENT_PEN);
	}
	else
	{
		dc.SetBrush(wxBrush(GetBackgroundColour()));
		dc.SetPen(*wxTRANSPARENT_PEN);
	}
	dc.DrawRectangle(rect);
}

void CommandPaletteListBox::OnDrawItem(wxDC& dc, const wxRect& rect, size_t n) const
{
	if (n >= _palette->_filtered.size()) return;

	const auto& entry = _palette->_allCommands[_palette->_filtered[n]];
	bool selected = IsSelected(n);

	wxColour titleColour = selected
		? wxSystemSettings::GetColour(wxSYS_COLOUR_HIGHLIGHTTEXT)
		: GetForegroundColour();
	wxColour descColour = selected
		? wxSystemSettings::GetColour(wxSYS_COLOUR_HIGHLIGHTTEXT)
		: wxSystemSettings::GetColour(wxSYS_COLOUR_GRAYTEXT);
	wxColour shortcutColour = descColour;

	wxFont titleFont = GetFont();
	wxFont descFont = GetFont();
	descFont.SetPointSize(descFont.GetPointSize() - 1);
	wxFont shortcutFont = descFont;

	int x = rect.GetLeft() + TEXT_LEFT_MARGIN;
	int rightEdge = rect.GetRight() - TEXT_RIGHT_MARGIN;

	// Measure shortcut width to reserve space
	int shortcutWidth = 0;
	if (!entry.shortcut.empty())
	{
		dc.SetFont(shortcutFont);
		shortcutWidth = dc.GetTextExtent(entry.shortcut).GetWidth() + SHORTCUT_RIGHT_MARGIN;
	}
	int textRight = rightEdge - shortcutWidth;

	// Draw title
	dc.SetFont(titleFont);
	dc.SetTextForeground(titleColour);

	wxString titleText = entry.displayName;
	titleText = wxControl::Ellipsize(titleText, dc, wxELLIPSIZE_END, textRight - x);
	wxSize titleSize = dc.GetTextExtent(titleText);

	int titleY = rect.GetTop() + ROW_PADDING;
	dc.DrawText(titleText, x, titleY);

	// Draw description below title
	if (!entry.description.empty())
	{
		dc.SetFont(descFont);
		dc.SetTextForeground(descColour);

		wxString descText = entry.description;
		descText = wxControl::Ellipsize(descText, dc, wxELLIPSIZE_END, textRight - x);

		int descY = titleY + titleSize.GetHeight() + DESC_TOP_GAP;
		dc.DrawText(descText, x, descY);
	}

	// Draw shortcut, vertically centered
	if (!entry.shortcut.empty())
	{
		dc.SetFont(shortcutFont);
		dc.SetTextForeground(shortcutColour);

		wxSize scSize = dc.GetTextExtent(entry.shortcut);
		int scX = rightEdge - scSize.GetWidth();
		int scY = rect.GetTop() + (rect.GetHeight() - scSize.GetHeight()) / 2;
		dc.DrawText(entry.shortcut, scX, scY);
	}
}

wxCoord CommandPaletteListBox::OnMeasureItem(size_t n) const
{
	if (n >= _palette->_filtered.size()) return 30;

	const auto& entry = _palette->_allCommands[_palette->_filtered[n]];

	wxFont titleFont = GetFont();
	wxFont descFont = GetFont();
	descFont.SetPointSize(descFont.GetPointSize() - 1);

	// Use a temp memory DC to measure text
	wxBitmap bmp(1, 1);
	wxMemoryDC dc(bmp);

	dc.SetFont(titleFont);
	int titleH = dc.GetTextExtent("Xg").GetHeight();

	int totalH = ROW_PADDING + titleH + ROW_PADDING;

	if (!entry.description.empty())
	{
		dc.SetFont(descFont);
		int descH = dc.GetTextExtent("Xg").GetHeight();
		totalH = ROW_PADDING + titleH + DESC_TOP_GAP + descH + ROW_PADDING;
	}

	return totalH;
}

CommandPalette::CommandPalette(wxWindow* parent) :
	DialogBase("", parent)
{
	SetWindowStyleFlag(wxBORDER_SIMPLE | wxSTAY_ON_TOP);
	SetBackgroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOW));

	auto* sizer = new wxBoxSizer(wxVERTICAL);

	_searchBox = new wxTextCtrl(this, wxID_ANY, wxEmptyString,
		wxDefaultPosition, wxDefaultSize, wxTE_PROCESS_ENTER);
	_searchBox->SetHint(_("Type a command..."));

	auto searchFont = _searchBox->GetFont();
	searchFont.SetPointSize(searchFont.GetPointSize() + 2);
	_searchBox->SetFont(searchFont);

	sizer->Add(_searchBox, 0, wxEXPAND | wxALL, 4);

	_list = new CommandPaletteListBox(this);
	sizer->Add(_list, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 4);

	SetSizer(sizer);

	populateCommands();
	applyFilter("");
	updateList();

	_searchBox->Bind(wxEVT_TEXT, &CommandPalette::onSearchChanged, this);
	_searchBox->Bind(wxEVT_KEY_DOWN, &CommandPalette::onKeyDown, this);
	_list->Bind(wxEVT_LISTBOX_DCLICK, [this](wxCommandEvent&) { executeSelected(); });
	Bind(wxEVT_ACTIVATE, &CommandPalette::onDeactivate, this);

	_searchBox->SetFocus();
}

void CommandPalette::populateCommands()
{
	_allCommands.clear();

	const auto& metadata = getCommandMetadata();

	class Collector : public IEventVisitor
	{
	public:
		std::vector<CommandEntry>& entries;
		const std::unordered_map<std::string, CommandInfo>& meta;

		Collector(std::vector<CommandEntry>& e,
				  const std::unordered_map<std::string, CommandInfo>& m)
			: entries(e), meta(m) {}

		void visit(const std::string& eventName, const IAccelerator& accel) override
		{
			if (eventName.empty() || eventName[0] == '_')
				return;

			std::string shortcut = accel.getString(true);
			std::replace(shortcut.begin(), shortcut.end(), '~', '+');

			std::string displayName;
			std::string description;
			CmdCategory category = CmdCategory::General;

			auto it = meta.find(eventName);
			if (it != meta.end())
			{
				displayName = it->second.displayName;
				description = it->second.description;
				category = it->second.category;
			}
			else
			{
				displayName = camelCaseToWords(eventName);
			}

			entries.push_back({
				eventName,
				std::move(displayName),
				std::move(description),
				std::move(shortcut),
				category,
				GlobalCommandSystem().canExecute(eventName)
			});
		}
	};

	Collector collector(_allCommands, metadata);
	GlobalEventManager().foreachEvent(collector);

	const auto& info = GlobalSelectionSystem().getSelectionInfo();

	auto scoreCategory = [&](CmdCategory cat) -> int
	{
		switch (cat)
		{
		case CmdCategory::Brush:
			return info.brushCount > 0 ? 2 : 0;
		case CmdCategory::Patch:
			return info.patchCount > 0 ? 2 : 0;
		case CmdCategory::Entity:
			return info.entityCount > 0 ? 2 : (info.totalCount > 0 ? 1 : 0);
		case CmdCategory::Texture:
			return (info.brushCount > 0 || info.patchCount > 0 || info.componentCount > 0) ? 1 : 0;
		case CmdCategory::Transform:
			return info.totalCount > 0 ? 1 : 0;
		case CmdCategory::Selection:
			return info.totalCount > 0 ? 1 : 0;
		default:
			return 0;
		}
	};

	for (auto& cmd : _allCommands)
		cmd.relevance = scoreCategory(cmd.category);

	std::sort(_allCommands.begin(), _allCommands.end(),
		[](const CommandEntry& a, const CommandEntry& b)
		{
			if (a.relevance != b.relevance)
				return a.relevance > b.relevance;
			return a.displayName < b.displayName;
		});
}

bool CommandPalette::fuzzyMatch(const std::string& text, const std::string& pattern)
{
	if (pattern.empty()) return true;

	auto pi = pattern.begin();

	for (auto ti = text.begin(); ti != text.end() && pi != pattern.end(); ++ti)
	{
		if (std::tolower(static_cast<unsigned char>(*ti)) ==
			std::tolower(static_cast<unsigned char>(*pi)))
		{
			++pi;
		}
	}

	return pi == pattern.end();
}

void CommandPalette::applyFilter(const std::string& text)
{
	_filtered.clear();

	for (int i = 0; i < static_cast<int>(_allCommands.size()); ++i)
	{
		const auto& cmd = _allCommands[i];
		if (!cmd.canExecute) continue;

		if (fuzzyMatch(cmd.displayName, text) ||
			fuzzyMatch(cmd.description, text) ||
			fuzzyMatch(cmd.internalName, text))
		{
			_filtered.push_back(i);
		}
	}
}

void CommandPalette::updateList()
{
	_list->SetItemCount(_filtered.size());
	_list->Refresh();

	if (!_filtered.empty())
		_list->SetSelection(0);
}

void CommandPalette::executeSelected()
{
	int sel = _list->GetSelection();
	if (sel < 0 || sel >= static_cast<int>(_filtered.size()))
		return;

	const auto& entry = _allCommands[_filtered[sel]];

	EndModal(wxID_OK);

	GlobalCommandSystem().execute(entry.internalName);
}

void CommandPalette::onSearchChanged(wxCommandEvent& ev)
{
	applyFilter(_searchBox->GetValue().ToStdString());
	updateList();
}

void CommandPalette::onKeyDown(wxKeyEvent& ev)
{
	int key = ev.GetKeyCode();

	if (key == WXK_DOWN || key == WXK_UP)
	{
		int sel = _list->GetSelection();
		int count = static_cast<int>(_filtered.size());
		if (count == 0) return;

		int next = (key == WXK_DOWN) ? std::min(sel + 1, count - 1)
		                             : std::max(sel - 1, 0);

		if (next != sel)
			_list->SetSelection(next);

		return;
	}

	if (key == WXK_RETURN || key == WXK_NUMPAD_ENTER)
	{
		executeSelected();
		return;
	}

	ev.Skip();
}

void CommandPalette::onDeactivate(wxActivateEvent& ev)
{
	if (!ev.GetActive())
	{
		EndModal(wxID_CANCEL);
	}
}

void CommandPalette::ShowPalette(const cmd::ArgumentList& args)
{
	wxFrame* mainFrame = GlobalMainFrame().getWxTopLevelWindow();
	if (!mainFrame) return;

	CommandPalette dlg(mainFrame);

	wxSize paletteSize(600, 400);
	dlg.SetSize(paletteSize);

	wxRect frameRect = mainFrame->GetScreenRect();
	int x = frameRect.GetLeft() + (frameRect.GetWidth() - paletteSize.GetWidth()) / 2;
	int y = frameRect.GetTop() + (frameRect.GetHeight() - paletteSize.GetHeight()) / 3;
	dlg.SetPosition(wxPoint(x, y));

	dlg.ShowModal();
	dlg.Destroy();
}

} // namespace ui
