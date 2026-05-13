#pragma once

#include "wxutil/dialog/DialogBase.h"
#include "wxutil/dataview/TreeModel.h"
#include "wxutil/dataview/TreeView.h"

#include <map>
#include <set>
#include <string>

namespace ui
{

class MapConversionDialog : public wxutil::DialogBase
{
public:
	struct MappingResult
	{
		std::map<std::string, std::string> textureMappings;
		std::map<std::string, std::string> entityMappings;
		std::set<std::string> entitiesToSkip;
	};

	static MappingResult RunDialog(wxWindow* parent,
		const std::string& formatName,
		const std::set<std::string>& sourceTextures,
		const std::set<std::string>& sourceEntities);

private:
	struct TextureColumns : public wxutil::TreeModel::ColumnRecord
	{
		wxutil::TreeModel::Column sourceTexture;
		wxutil::TreeModel::Column targetTexture;
		wxutil::TreeModel::Column matchType;

		TextureColumns() :
			sourceTexture(add(wxutil::TreeModel::Column::String)),
			targetTexture(add(wxutil::TreeModel::Column::String)),
			matchType(add(wxutil::TreeModel::Column::String))
		{}
	};

	struct EntityColumns : public wxutil::TreeModel::ColumnRecord
	{
		wxutil::TreeModel::Column sourceEntity;
		wxutil::TreeModel::Column targetEntity;
		wxutil::TreeModel::Column matchType;
		wxutil::TreeModel::Column skipImport;

		EntityColumns() :
			sourceEntity(add(wxutil::TreeModel::Column::String)),
			targetEntity(add(wxutil::TreeModel::Column::String)),
			matchType(add(wxutil::TreeModel::Column::String)),
			skipImport(add(wxutil::TreeModel::Column::Boolean))
		{}
	};

	TextureColumns _texColumns;
	EntityColumns _entColumns;

	wxutil::TreeModel* _texStore;
	wxutil::TreeView* _texView;
	wxutil::TreeModel* _entStore;
	wxutil::TreeView* _entView;

	std::map<std::string, std::string> _knownTextures;
	std::map<std::string, std::string> _knownEntities;

	MappingResult _result;

	MapConversionDialog(wxWindow* parent, const std::string& formatName);

	void buildKnownMaps();

	void populate(const std::set<std::string>& sourceTextures,
				  const std::set<std::string>& sourceEntities);

	std::string findTextureMatch(const std::string& source) const;
	std::string findEntityMatch(const std::string& source) const;

	void onDefaultAll(wxCommandEvent& ev);
	void onOK(wxCommandEvent& ev);
	void onSkip(wxCommandEvent& ev);
	void onBrowseTexture(wxCommandEvent& ev);
	void onBrowseEntity(wxCommandEvent& ev);
	void onSavePreset(wxCommandEvent& ev);
	void onLoadPreset(wxCommandEvent& ev);
};

} // namespace ui
