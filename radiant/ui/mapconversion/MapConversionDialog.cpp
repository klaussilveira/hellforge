#include "MapConversionDialog.h"

#include "i18n.h"
#include "itextstream.h"
#include "ui/materials/MaterialChooser.h"
#include "ui/materials/MaterialSelector.h"

#include <fmt/format.h>

#include <wx/sizer.h>
#include <wx/button.h>
#include <wx/stattext.h>
#include <wx/notebook.h>
#include <wx/panel.h>
#include <wx/filedlg.h>

#include <fstream>

#include "wxutil/EntityClassChooser.h"
#include "ui/imainframe.h"

#include <algorithm>
#include <cctype>

namespace ui
{

namespace
{

const std::map<std::string, std::string> KNOWN_TEXTURE_MAP = {
	{ "tools/toolsnodraw", "textures/common/nodraw" },
	{ "tools/toolsclip", "textures/common/clip" },
	{ "tools/toolsinvisible", "textures/common/clip" },
	{ "tools/toolsplayerclip", "textures/common/player_clip" },
	{ "tools/toolsnpcclip", "textures/common/monster_clip" },
	{ "tools/toolstrigger", "textures/common/trigger" },
	{ "tools/toolsskip", "textures/common/nodraw" },
	{ "tools/toolshint", "textures/common/hint" },
	{ "tools/toolsareaportal", "textures/editor/visportal" },
	{ "tools/toolsoccluder", "textures/common/nodraw" },
	{ "tools/toolsblocklight", "textures/common/shadow" },
	{ "tools/toolsblockbullets", "textures/common/clip" },
	{ "tools/toolsblocklos", "textures/common/clip" },
	{ "tools/toolsskybox", "textures/common/nodraw" },
	{ "tools/toolsfog", "textures/common/nodraw" },

	{ "clip", "textures/common/clip" },
	{ "trigger", "textures/common/trigger" },
	{ "skip", "textures/common/nodraw" },
	{ "hint", "textures/common/hint" },
	{ "hintskip", "textures/common/nodraw" },
	{ "origin", "textures/common/origin" },
	{ "areaportal", "textures/editor/visportal" },
	{ "common/caulk", "textures/common/caulk" },
	{ "caulk", "textures/common/caulk" },
	{ "nodraw", "textures/common/nodraw" },
	{ "noclip", "textures/common/nodraw" },
	{ "playerclip", "textures/common/player_clip" },
	{ "common/clip", "textures/common/clip" },
	{ "common/trigger", "textures/common/trigger" },
	{ "common/skip", "textures/common/nodraw" },
	{ "common/hint", "textures/common/hint" },
	{ "common/nodraw", "textures/common/nodraw" },
	{ "common/origin", "textures/common/origin" },
	{ "common/areaportal", "textures/editor/visportal" },
};

const std::map<std::string, std::string> KNOWN_ENTITY_MAP = {
	{ "worldspawn", "worldspawn" },
	{ "info_player_start", "info_player_start" },
	{ "info_player_deathmatch", "info_player_start" },
	{ "info_player_coop", "info_player_start" },
	{ "info_player_terrorist", "info_player_start" },
	{ "info_player_counterterrorist", "info_player_start" },
	{ "light", "light" },
	{ "light_spot", "light" },
	{ "light_environment", "light" },
	{ "func_wall", "func_static" },
	{ "func_illusionary", "func_static" },
	{ "func_detail", "func_static" },
	{ "func_group", "func_static" },
	{ "func_door", "func_door" },
	{ "func_door_rotating", "func_door" },
	{ "func_button", "func_door" },
	{ "func_plat", "func_plat" },
	{ "func_rotating", "func_rotating" },
	{ "func_breakable", "func_fracture" },
	{ "trigger_once", "trigger_once" },
	{ "trigger_multiple", "trigger_multiple" },
	{ "trigger_hurt", "trigger_hurt" },
	{ "trigger_push", "trigger_push" },
	{ "trigger_teleport", "trigger_teleport" },
	{ "ambient_generic", "speaker" },
	{ "env_sound", "speaker" },
	{ "info_target", "target_null" },
	{ "info_null", "target_null" },
	{ "info_notnull", "target_null" },
	{ "path_corner", "path_corner" },
};

std::string toLower(const std::string& s)
{
	std::string result = s;
	std::transform(result.begin(), result.end(), result.begin(),
		[](unsigned char c) { return std::tolower(c); });
	return result;
}

std::string extractBaseName(const std::string& fullName)
{
	auto pos = fullName.rfind('/');
	if (pos != std::string::npos)
		return fullName.substr(pos + 1);
	pos = fullName.rfind('\\');
	if (pos != std::string::npos)
		return fullName.substr(pos + 1);
	return fullName;
}

std::string stripPrefix(const std::string& name)
{
	std::string lower = toLower(name);
	if (lower.substr(0, 9) == "textures/")
		return lower.substr(9);
	return lower;
}

}

MapConversionDialog::MapConversionDialog(wxWindow* parent, const std::string& formatName) :
	DialogBase(fmt::format("Convert {} Map", formatName), parent)
{
	auto* mainSizer = new wxBoxSizer(wxVERTICAL);

	auto* infoLabel = new wxStaticText(this, wxID_ANY,
		fmt::format("The loaded map uses the {} format.\n"
		"You can remap textures and entities to match the current game.", formatName));
	mainSizer->Add(infoLabel, 0, wxALL, 10);

	auto* notebook = new wxNotebook(this, wxID_ANY);

	auto* texPanel = new wxPanel(notebook);
	auto* texSizer = new wxBoxSizer(wxVERTICAL);

	_texStore = new wxutil::TreeModel(_texColumns, true);
	_texView = wxutil::TreeView::CreateWithModel(texPanel, _texStore);

	_texView->AppendTextColumn(_("Source Texture"), _texColumns.sourceTexture.getColumnIndex(),
		wxDATAVIEW_CELL_INERT, wxCOL_WIDTH_AUTOSIZE, wxALIGN_NOT, wxDATAVIEW_COL_SORTABLE);
	_texView->AppendTextColumn(_("Target Texture"), _texColumns.targetTexture.getColumnIndex(),
		wxDATAVIEW_CELL_EDITABLE, wxCOL_WIDTH_AUTOSIZE, wxALIGN_NOT, wxDATAVIEW_COL_SORTABLE);
	_texView->AppendTextColumn(_("Match"), _texColumns.matchType.getColumnIndex(),
		wxDATAVIEW_CELL_INERT, wxCOL_WIDTH_AUTOSIZE, wxALIGN_NOT, wxDATAVIEW_COL_SORTABLE);

	texSizer->Add(_texView, 1, wxEXPAND | wxALL, 5);

	auto* texBtnSizer = new wxBoxSizer(wxHORIZONTAL);

	auto* browseTexBtn = new wxButton(texPanel, wxID_ANY, _("Find Material..."));
	browseTexBtn->Bind(wxEVT_BUTTON, &MapConversionDialog::onBrowseTexture, this);
	texBtnSizer->Add(browseTexBtn, 0, wxALL, 5);

	auto* defaultBtn = new wxButton(texPanel, wxID_ANY, _("Set All Unmapped to _default"));
	defaultBtn->Bind(wxEVT_BUTTON, &MapConversionDialog::onDefaultAll, this);
	texBtnSizer->Add(defaultBtn, 0, wxALL, 5);

	texSizer->Add(texBtnSizer, 0);

	texPanel->SetSizer(texSizer);
	notebook->AddPage(texPanel, _("Textures"));

	auto* entPanel = new wxPanel(notebook);
	auto* entSizer = new wxBoxSizer(wxVERTICAL);

	_entStore = new wxutil::TreeModel(_entColumns, true);
	_entView = wxutil::TreeView::CreateWithModel(entPanel, _entStore);

	_entView->AppendTextColumn(_("Source Entity"), _entColumns.sourceEntity.getColumnIndex(),
		wxDATAVIEW_CELL_INERT, wxCOL_WIDTH_AUTOSIZE, wxALIGN_NOT, wxDATAVIEW_COL_SORTABLE);
	_entView->AppendTextColumn(_("Target Entity"), _entColumns.targetEntity.getColumnIndex(),
		wxDATAVIEW_CELL_EDITABLE, wxCOL_WIDTH_AUTOSIZE, wxALIGN_NOT, wxDATAVIEW_COL_SORTABLE);
	_entView->AppendTextColumn(_("Match"), _entColumns.matchType.getColumnIndex(),
		wxDATAVIEW_CELL_INERT, wxCOL_WIDTH_AUTOSIZE, wxALIGN_NOT, wxDATAVIEW_COL_SORTABLE);

	auto* browseEntBtn = new wxButton(entPanel, wxID_ANY, _("Find Entity Class..."));
	browseEntBtn->Bind(wxEVT_BUTTON, &MapConversionDialog::onBrowseEntity, this);

	entSizer->Add(_entView, 1, wxEXPAND | wxALL, 5);
	entSizer->Add(browseEntBtn, 0, wxALL, 5);
	entPanel->SetSizer(entSizer);
	notebook->AddPage(entPanel, _("Entities"));

	mainSizer->Add(notebook, 1, wxEXPAND | wxALL, 5);

	auto* btnSizer = new wxBoxSizer(wxHORIZONTAL);

	auto* loadPresetBtn = new wxButton(this, wxID_ANY, _("Load Preset..."));
	loadPresetBtn->Bind(wxEVT_BUTTON, &MapConversionDialog::onLoadPreset, this);
	btnSizer->Add(loadPresetBtn, 0, wxALL, 5);

	auto* savePresetBtn = new wxButton(this, wxID_ANY, _("Save Preset..."));
	savePresetBtn->Bind(wxEVT_BUTTON, &MapConversionDialog::onSavePreset, this);
	btnSizer->Add(savePresetBtn, 0, wxALL, 5);

	btnSizer->AddStretchSpacer();

	auto* skipBtn = new wxButton(this, wxID_CANCEL, _("Skip"));
	skipBtn->Bind(wxEVT_BUTTON, &MapConversionDialog::onSkip, this);
	btnSizer->Add(skipBtn, 0, wxALL, 5);

	auto* okBtn = new wxButton(this, wxID_OK, _("Apply Mappings"));
	okBtn->Bind(wxEVT_BUTTON, &MapConversionDialog::onOK, this);
	btnSizer->Add(okBtn, 0, wxALL, 5);

	mainSizer->Add(btnSizer, 0, wxEXPAND | wxBOTTOM | wxRIGHT, 5);

	SetSizer(mainSizer);
	FitToScreen(0.6f, 0.7f);
}

std::string MapConversionDialog::findTextureMatch(const std::string& source) const
{
	std::string sourceLower = toLower(source);
	std::string sourceStripped = stripPrefix(sourceLower);
	std::string sourceBase = toLower(extractBaseName(source));

	for (const auto& [key, value] : KNOWN_TEXTURE_MAP)
	{
		if (sourceLower == key || sourceStripped == key || sourceBase == key)
			return value;
	}

	return {};
}

std::string MapConversionDialog::findEntityMatch(const std::string& source) const
{
	std::string sourceLower = toLower(source);

	for (const auto& [key, value] : KNOWN_ENTITY_MAP)
	{
		if (sourceLower == key)
			return value;
	}

	return {};
}

void MapConversionDialog::populate(const std::set<std::string>& sourceTextures,
								   const std::set<std::string>& sourceEntities)
{
	for (const auto& tex : sourceTextures)
	{
		wxutil::TreeModel::Row row = _texStore->AddItem();
		std::string match = findTextureMatch(tex);

		row[_texColumns.sourceTexture] = tex;
		row[_texColumns.targetTexture] = match;
		row[_texColumns.matchType] = match.empty() ? std::string("") : std::string("Known");

		row.SendItemAdded();
	}

	for (const auto& ent : sourceEntities)
	{
		wxutil::TreeModel::Row row = _entStore->AddItem();
		std::string match = findEntityMatch(ent);

		row[_entColumns.sourceEntity] = ent;
		row[_entColumns.targetEntity] = match;
		row[_entColumns.matchType] = match.empty() ? std::string("") : std::string("Known");

		row.SendItemAdded();
	}
}

void MapConversionDialog::onBrowseTexture(wxCommandEvent& ev)
{
	auto item = _texView->GetSelection();
	if (!item.IsOk()) return;

	wxutil::TreeModel::Row row(item, *_texStore);
	std::string current = row[_texColumns.targetTexture];

	MaterialChooser chooser(GlobalMainFrame().getWxTopLevelWindow(),
		MaterialSelector::TextureFilter::Regular);
	if (!current.empty())
	{
		chooser.SetSelectedDeclName(current);
	}

	if (chooser.ShowModal() == wxID_OK)
	{
		std::string selected = chooser.GetSelectedDeclName();
		if (!selected.empty())
		{
			row[_texColumns.targetTexture] = selected;
			row[_texColumns.matchType] = std::string("Manual");
			row.SendItemChanged();
		}
	}
}

void MapConversionDialog::onBrowseEntity(wxCommandEvent& ev)
{
	auto item = _entView->GetSelection();
	if (!item.IsOk()) return;

	wxutil::TreeModel::Row row(item, *_entStore);
	std::string current = row[_entColumns.targetEntity];

	std::string selected = wxutil::EntityClassChooser::ChooseEntityClass(
		wxutil::EntityClassChooser::Purpose::SelectClassname, current);

	if (!selected.empty())
	{
		row[_entColumns.targetEntity] = selected;
		row[_entColumns.matchType] = std::string("Manual");
		row.SendItemChanged();
	}
}

void MapConversionDialog::onDefaultAll(wxCommandEvent& ev)
{
	_texStore->ForeachNode([this](wxutil::TreeModel::Row& row)
	{
		std::string target = row[_texColumns.targetTexture];
		if (target.empty())
		{
			row[_texColumns.targetTexture] = std::string("textures/_default");
			row[_texColumns.matchType] = std::string("Default");
			row.SendItemChanged();
		}
	});
}

void MapConversionDialog::onSavePreset(wxCommandEvent& ev)
{
	wxFileDialog dlg(this, _("Save Mapping Preset"), "", "", "Import Mapping Files (*.imf)|*.imf",
		wxFD_SAVE | wxFD_OVERWRITE_PROMPT);

	if (dlg.ShowModal() != wxID_OK) return;

	std::ofstream file(dlg.GetPath().ToStdString());
	if (!file.good()) return;

	file << "[textures]" << std::endl;
	_texStore->ForeachNode([&](wxutil::TreeModel::Row& row)
	{
		std::string source = row[_texColumns.sourceTexture];
		std::string target = row[_texColumns.targetTexture];
		if (!target.empty())
		{
			file << source << "," << target << std::endl;
		}
	});

	file << "[entities]" << std::endl;
	_entStore->ForeachNode([&](wxutil::TreeModel::Row& row)
	{
		std::string source = row[_entColumns.sourceEntity];
		std::string target = row[_entColumns.targetEntity];
		if (!target.empty())
		{
			file << source << "," << target << std::endl;
		}
	});
}

void MapConversionDialog::onLoadPreset(wxCommandEvent& ev)
{
	wxFileDialog dlg(this, _("Load Mapping Preset"), "", "", "Import Mapping Files (*.imf)|*.imf",
		wxFD_OPEN | wxFD_FILE_MUST_EXIST);

	if (dlg.ShowModal() != wxID_OK) return;

	std::ifstream file(dlg.GetPath().ToStdString());
	if (!file.good()) return;

	std::map<std::string, std::string> texMappings;
	std::map<std::string, std::string> entMappings;

	enum Section { None, Textures, Entities } section = None;

	std::string line;
	while (std::getline(file, line))
	{
		if (line.empty()) continue;

		if (line == "[textures]") { section = Textures; continue; }
		if (line == "[entities]") { section = Entities; continue; }

		auto comma = line.find(',');
		if (comma == std::string::npos) continue;

		std::string source = line.substr(0, comma);
		std::string target = line.substr(comma + 1);

		if (source.empty() || target.empty()) continue;

		if (section == Textures)
			texMappings[source] = target;
		else if (section == Entities)
			entMappings[source] = target;
	}

	_texStore->ForeachNode([&](wxutil::TreeModel::Row& row)
	{
		std::string source = row[_texColumns.sourceTexture];
		auto it = texMappings.find(source);
		if (it != texMappings.end())
		{
			row[_texColumns.targetTexture] = it->second;
			row[_texColumns.matchType] = std::string("Preset");
			row.SendItemChanged();
		}
	});

	_entStore->ForeachNode([&](wxutil::TreeModel::Row& row)
	{
		std::string source = row[_entColumns.sourceEntity];
		auto it = entMappings.find(source);
		if (it != entMappings.end())
		{
			row[_entColumns.targetEntity] = it->second;
			row[_entColumns.matchType] = std::string("Preset");
			row.SendItemChanged();
		}
	});
}

void MapConversionDialog::onOK(wxCommandEvent& ev)
{
	_texStore->ForeachNode([this](wxutil::TreeModel::Row& row)
	{
		std::string source = row[_texColumns.sourceTexture];
		std::string target = row[_texColumns.targetTexture];
		if (!target.empty() && source != target)
		{
			_result.textureMappings[source] = target;
		}
	});

	_entStore->ForeachNode([this](wxutil::TreeModel::Row& row)
	{
		std::string source = row[_entColumns.sourceEntity];
		std::string target = row[_entColumns.targetEntity];
		if (!target.empty() && source != target)
		{
			_result.entityMappings[source] = target;
		}
	});

	EndModal(wxID_OK);
}

void MapConversionDialog::onSkip(wxCommandEvent& ev)
{
	EndModal(wxID_CANCEL);
}

MapConversionDialog::MappingResult MapConversionDialog::RunDialog(
	wxWindow* parent,
	const std::string& formatName,
	const std::set<std::string>& sourceTextures,
	const std::set<std::string>& sourceEntities)
{
	MapConversionDialog dlg(parent, formatName);
	dlg.populate(sourceTextures, sourceEntities);

	if (dlg.ShowModal() == wxID_OK)
	{
		return dlg._result;
	}

	return {};
}

} // namespace ui
