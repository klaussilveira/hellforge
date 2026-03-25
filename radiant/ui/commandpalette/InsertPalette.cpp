#include "InsertPalette.h"

#include "i18n.h"
#include "icommandsystem.h"
#include "command/ExecutionFailure.h"
#include "ientity.h"
#include "scene/EntityNode.h"
#include "ieclass.h"
#include "iparticles.h"
#include "ifilesystem.h"
#include "imap.h"
#include "iselection.h"
#include "icameraview.h"
#include "igrid.h"
#include "iorthoview.h"
#include "iscenegraph.h"
#include "scenelib.h"
#include "iselectable.h"
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

constexpr int ROW_PADDING = 6;
constexpr int TEXT_LEFT_MARGIN = 8;
constexpr int TEXT_RIGHT_MARGIN = 8;
constexpr int TYPE_RIGHT_MARGIN = 12;
constexpr int DESC_TOP_GAP = 1;

} // namespace

InsertPaletteListBox::InsertPaletteListBox(InsertPalette* parent) :
	wxVListBox(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxBORDER_NONE),
	_palette(parent)
{
}

void InsertPaletteListBox::OnDrawBackground(wxDC& dc, const wxRect& rect, size_t n) const
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

void InsertPaletteListBox::OnDrawItem(wxDC& dc, const wxRect& rect, size_t n) const
{
	if (n >= _palette->_filtered.size()) return;

	bool selected = IsSelected(n);

	wxColour titleColour = selected
		? wxSystemSettings::GetColour(wxSYS_COLOUR_HIGHLIGHTTEXT)
		: GetForegroundColour();
	wxColour descColour = selected
		? wxSystemSettings::GetColour(wxSYS_COLOUR_HIGHLIGHTTEXT)
		: wxSystemSettings::GetColour(wxSYS_COLOUR_GRAYTEXT);
	wxColour typeColour = descColour;

	wxFont titleFont = GetFont();
	wxFont descFont = GetFont();
	descFont.SetPointSize(descFont.GetPointSize() - 1);
	wxFont typeFont = descFont;

	int x = rect.GetLeft() + TEXT_LEFT_MARGIN;
	int rightEdge = rect.GetRight() - TEXT_RIGHT_MARGIN;

	if (_palette->_goToMode)
	{
		const auto& ent = _palette->_sceneEntities[_palette->_filtered[n]];

		wxString typeLbl = wxString::Format("Entity %zu", ent.index);
		dc.SetFont(typeFont);
		int typeWidth = dc.GetTextExtent(typeLbl).GetWidth() + TYPE_RIGHT_MARGIN;
		int textRight = rightEdge - typeWidth;

		dc.SetFont(titleFont);
		dc.SetTextForeground(titleColour);
		wxString titleText = ent.name;
		titleText = wxControl::Ellipsize(titleText, dc, wxELLIPSIZE_END, textRight - x);
		int titleY = rect.GetTop() + (rect.GetHeight() - dc.GetTextExtent(titleText).GetHeight()) / 2;
		dc.DrawText(titleText, x, titleY);

		dc.SetFont(typeFont);
		dc.SetTextForeground(typeColour);
		wxSize tlSize = dc.GetTextExtent(typeLbl);
		int tlX = rightEdge - tlSize.GetWidth();
		int tlY = rect.GetTop() + (rect.GetHeight() - tlSize.GetHeight()) / 2;
		dc.DrawText(typeLbl, tlX, tlY);

		return;
	}

	const auto& entry = _palette->_allAssets[_palette->_filtered[n]];

	// Measure type label width to reserve space
	const char* typeLbl = InsertPalette::typeLabel(entry.type);
	int typeWidth = 0;
	if (typeLbl[0] != '\0')
	{
		dc.SetFont(typeFont);
		typeWidth = dc.GetTextExtent(typeLbl).GetWidth() + TYPE_RIGHT_MARGIN;
	}
	int textRight = rightEdge - typeWidth;

	// Draw title (asset name)
	dc.SetFont(titleFont);
	dc.SetTextForeground(titleColour);

	wxString titleText = entry.name;
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

	// Draw type label, vertically centered
	if (typeLbl[0] != '\0')
	{
		dc.SetFont(typeFont);
		dc.SetTextForeground(typeColour);

		wxSize tlSize = dc.GetTextExtent(typeLbl);
		int tlX = rightEdge - tlSize.GetWidth();
		int tlY = rect.GetTop() + (rect.GetHeight() - tlSize.GetHeight()) / 2;
		dc.DrawText(typeLbl, tlX, tlY);
	}
}

wxCoord InsertPaletteListBox::OnMeasureItem(size_t n) const
{
	if (n >= _palette->_filtered.size()) return 30;

	wxFont titleFont = GetFont();

	wxBitmap bmp(1, 1);
	wxMemoryDC dc(bmp);

	dc.SetFont(titleFont);
	int titleH = dc.GetTextExtent("Xg").GetHeight();

	if (_palette->_goToMode)
		return ROW_PADDING + titleH + ROW_PADDING;

	const auto& entry = _palette->_allAssets[_palette->_filtered[n]];

	int totalH = ROW_PADDING + titleH + ROW_PADDING;

	if (!entry.description.empty())
	{
		wxFont descFont = GetFont();
		descFont.SetPointSize(descFont.GetPointSize() - 1);
		dc.SetFont(descFont);
		int descH = dc.GetTextExtent("Xg").GetHeight();
		totalH = ROW_PADDING + titleH + DESC_TOP_GAP + descH + ROW_PADDING;
	}

	return totalH;
}

InsertPalette::InsertPalette(wxWindow* parent) :
	DialogBase("", parent),
	_goToMode(false)
{
	SetWindowStyleFlag(wxBORDER_SIMPLE | wxSTAY_ON_TOP);
	SetBackgroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOW));

	auto* sizer = new wxBoxSizer(wxVERTICAL);

	_searchBox = new wxTextCtrl(this, wxID_ANY, wxEmptyString,
		wxDefaultPosition, wxDefaultSize, wxTE_PROCESS_ENTER);
	_searchBox->SetHint(_("Search entity to insert, or type : to go to entity..."));

	auto searchFont = _searchBox->GetFont();
	searchFont.SetPointSize(searchFont.GetPointSize() + 2);
	_searchBox->SetFont(searchFont);

	sizer->Add(_searchBox, 0, wxEXPAND | wxALL, 4);

	_list = new InsertPaletteListBox(this);
	sizer->Add(_list, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 4);

	SetSizer(sizer);

	populateAssets();
	applyFilter("");
	updateList();

	_searchBox->Bind(wxEVT_TEXT, &InsertPalette::onSearchChanged, this);
	_searchBox->Bind(wxEVT_KEY_DOWN, &InsertPalette::onKeyDown, this);
	_list->Bind(wxEVT_LISTBOX_DCLICK, [this](wxCommandEvent&) {
		if (_goToMode) goToSelected(); else insertSelected();
	});
	Bind(wxEVT_ACTIVATE, &InsertPalette::onDeactivate, this);

	_searchBox->SetFocus();
}

void InsertPalette::populateAssets()
{
	_allAssets.clear();

	// Entity classes (only point entities that can be placed without brushes)
	GlobalEntityClassManager().forEachEntityClass(
		[this](const scene::EntityClass::Ptr& eclass)
		{
			if (eclass->getVisibility() == vfs::Visibility::HIDDEN)
				return;

			// Skip brush-based entity classes - they need selected brushes
			if (!eclass->isFixedSize())
				return;

			std::string desc = eclass->getAttributeValue("editor_usage");
			if (desc.empty())
				desc = eclass->getAttributeValue("editor_description");

			_allAssets.push_back({
				eclass->getDeclName(),
				std::move(desc),
				AssetType::EntityClass,
			});
		});

	// Models
	auto addModel = [this](const vfs::FileInfo& file)
	{
		_allAssets.push_back({
			file.fullPath(),
			"",
			AssetType::Model,
		});
	};

	for (const char* ext : { "lwo", "ase", "md5mesh", "obj", "3ds", "dae" })
	{
		GlobalFileSystem().forEachFile("models/", ext, addModel, 99);
	}

	// Prefabs
	auto addPrefab = [this](const vfs::FileInfo& file)
	{
		_allAssets.push_back({
			file.fullPath(),
			"",
			AssetType::Prefab,
		});
	};

	for (const char* ext : { "pfb", "pfbx", "map" })
	{
		GlobalFileSystem().forEachFile("prefabs/", ext, addPrefab, 99);
	}

	// Particles
	GlobalParticlesManager().forEachParticleDef(
		[this](const particles::IParticleDef& def)
		{
			_allAssets.push_back({
				def.getDeclName(),
				"",
				AssetType::Particle,
			});
		});

	std::sort(_allAssets.begin(), _allAssets.end(),
		[](const AssetEntry& a, const AssetEntry& b)
		{
			return a.name < b.name;
		});
}

void InsertPalette::populateSceneEntities()
{
	_sceneEntities.clear();

	if (!GlobalMapModule().getRoot())
		return;

	std::size_t entityIndex = 0;

	GlobalSceneGraph().root()->foreachNode([this, &entityIndex](const scene::INodePtr& node) -> bool
	{
		if (Node_isEntity(node))
		{
			_sceneEntities.push_back({ node->name(), entityIndex });
			++entityIndex;
		}
		return true;
	});
}

void InsertPalette::applyGoToFilter(const std::string& text)
{
	_filtered.clear();

	bool isNumber = !text.empty() && std::all_of(text.begin(), text.end(),
		[](unsigned char c) { return std::isdigit(c); });

	if (isNumber)
	{
		std::size_t targetIndex = std::stoul(text);
		for (int i = 0; i < static_cast<int>(_sceneEntities.size()); ++i)
		{
			if (_sceneEntities[i].index == targetIndex)
			{
				_filtered.push_back(i);
				break;
			}
		}
	}
	else
	{
		for (int i = 0; i < static_cast<int>(_sceneEntities.size()); ++i)
		{
			const auto& ent = _sceneEntities[i];
			std::string indexStr = std::to_string(ent.index);
			if (fuzzyMatch(ent.name, text) || fuzzyMatch(indexStr, text))
				_filtered.push_back(i);
		}
	}
}

bool InsertPalette::fuzzyMatch(const std::string& text, const std::string& pattern)
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

void InsertPalette::applyFilter(const std::string& text)
{
	_filtered.clear();

	for (int i = 0; i < static_cast<int>(_allAssets.size()); ++i)
	{
		const auto& asset = _allAssets[i];

		if (fuzzyMatch(asset.name, text) ||
			fuzzyMatch(asset.description, text))
		{
			_filtered.push_back(i);
		}
	}
}

void InsertPalette::updateList()
{
	_list->SetItemCount(_filtered.size());
	_list->Refresh();

	if (!_filtered.empty())
		_list->SetSelection(0);
}

void InsertPalette::insertSelected()
{
	int sel = _list->GetSelection();
	if (sel < 0 || sel >= static_cast<int>(_filtered.size()))
		return;

	const auto& entry = _allAssets[_filtered[sel]];

	EndModal(wxID_OK);

	// Deselect everything first so the new entity is cleanly created
	GlobalSelectionSystem().setSelectedAll(false);

	// Place where the camera is pointing, snapped to grid
	auto& cam = GlobalCameraManager().getActiveView();
	Vector3 pos = (cam.getCameraOrigin() - cam.getForwardVector() * 256.0)
		.getSnapped(GlobalGrid().getGridSize());

	try
	{
		switch (entry.type)
		{
		case AssetType::EntityClass:
			GlobalEntityModule().createEntityFromSelection(entry.name, pos);
			break;

		case AssetType::Model:
		{
			auto node = GlobalEntityModule().createEntityFromSelection("func_static", pos);
			if (node)
			{
				node->getEntity().setKeyValue("model", entry.name);
			}
			break;
		}

		case AssetType::Prefab:
		{
			cmd::ArgumentList args;
			args.push_back(entry.name);
			args.push_back(pos);
			args.push_back(1); // insertAsGroup
			args.push_back(1); // recalculatePrefabOrigin
			GlobalCommandSystem().executeCommand("LoadPrefabAt", args);
			break;
		}

		case AssetType::Particle:
		{
			auto node = GlobalEntityModule().createEntityFromSelection("func_emitter", pos);
			if (node)
			{
				node->getEntity().setKeyValue("model", entry.name);
			}
			break;
		}
		}
	}
	catch (const cmd::ExecutionFailure&)
	{
		// Entity requires brushes or other preconditions not met
	}
}

void InsertPalette::goToSelected()
{
	int sel = _list->GetSelection();
	if (sel < 0 || sel >= static_cast<int>(_filtered.size()))
		return;

	const auto& ent = _sceneEntities[_filtered[sel]];

	EndModal(wxID_OK);

	std::size_t idx = ent.index;
	scene::INodePtr foundNode;
	GlobalSceneGraph().root()->foreachNode([&](const scene::INodePtr& node) -> bool
	{
		if (Node_isEntity(node) && idx-- == 0)
		{
			foundNode = node;
			return false;
		}
		return true;
	});

	if (foundNode)
	{
		GlobalSelectionSystem().setSelectedAll(false);
		Node_setSelected(foundNode, true);

		auto originAndAngles = scene::getOriginAndAnglesToLookAtNode(*foundNode);
		GlobalCommandSystem().executeCommand("FocusViews",
			cmd::ArgumentList{ originAndAngles.first, originAndAngles.second });
	}
}

const char* InsertPalette::typeLabel(AssetType type)
{
	switch (type)
	{
	case AssetType::EntityClass: return "Entity";
	case AssetType::Model:       return "Model";
	case AssetType::Prefab:      return "Prefab";
	case AssetType::Particle:    return "Particle";
	default:                     return "";
	}
}

void InsertPalette::onSearchChanged(wxCommandEvent& ev)
{
	std::string text = _searchBox->GetValue().ToStdString();

	if (!text.empty() && text[0] == ':')
	{
		if (!_goToMode)
		{
			_goToMode = true;
			populateSceneEntities();
		}
		applyGoToFilter(text.substr(1));
	}
	else
	{
		_goToMode = false;
		applyFilter(text);
	}

	updateList();
}

void InsertPalette::onKeyDown(wxKeyEvent& ev)
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
		if (_goToMode)
			goToSelected();
		else
			insertSelected();
		return;
	}

	ev.Skip();
}

void InsertPalette::onDeactivate(wxActivateEvent& ev)
{
	if (!ev.GetActive())
	{
		EndModal(wxID_CANCEL);
	}
}

void InsertPalette::ShowPalette(const cmd::ArgumentList& args)
{
	wxFrame* mainFrame = GlobalMainFrame().getWxTopLevelWindow();
	if (!mainFrame) return;

	InsertPalette dlg(mainFrame);

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
