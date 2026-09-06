#include "WadImportDialog.h"

#include "i18n.h"
#include "os/path.h"

#include <fmt/format.h>

#include <wx/button.h>
#include <wx/checkbox.h>
#include <wx/choice.h>
#include <wx/sizer.h>
#include <wx/spinctrl.h>
#include <wx/stattext.h>

namespace ui
{

WadImportDialog::WadImportDialog(wxWindow* parent, const std::string& wadPath,
	const std::vector<std::string>& mapNames, double defaultScale, double defaultLightSpacing) :
	DialogBase(_("Import Doom WAD"), parent)
{
	auto* mainSizer = new wxBoxSizer(wxVERTICAL);

	auto* infoLabel = new wxStaticText(this, wxID_ANY,
		fmt::format(_("Importing {0}\nSector geometry will be converted into brushes."),
			os::getFilename(wadPath)));
	mainSizer->Add(infoLabel, 0, wxALL, 10);

	auto* grid = new wxFlexGridSizer(2, 8, 12);
	grid->AddGrowableCol(1);

	grid->Add(new wxStaticText(this, wxID_ANY, _("Map:")), 0, wxALIGN_CENTER_VERTICAL);

	wxArrayString choices;

	for (const auto& name : mapNames)
	{
		choices.Add(name);
	}

	_mapChoice = new wxChoice(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, choices);
	_mapChoice->SetSelection(0);
	_mapChoice->Enable(mapNames.size() > 1);
	grid->Add(_mapChoice, 0, wxEXPAND);

	grid->Add(new wxStaticText(this, wxID_ANY, _("Scale:")), 0, wxALIGN_CENTER_VERTICAL);

	_scaleEntry = new wxSpinCtrlDouble(this, wxID_ANY);
	_scaleEntry->SetRange(0.1, 16.0);
	_scaleEntry->SetIncrement(0.05);
	_scaleEntry->SetDigits(4);
	_scaleEntry->SetValue(defaultScale);
	grid->Add(_scaleEntry, 0, wxEXPAND);

	grid->AddSpacer(0);
	grid->Add(new wxStaticText(this, wxID_ANY,
		_("The default matches a Doom eye height of 41 units to 68 units.")), 0, wxEXPAND);

	grid->Add(new wxStaticText(this, wxID_ANY, _("Lights:")), 0, wxALIGN_CENTER_VERTICAL);

	_generateLights = new wxCheckBox(this, wxID_ANY, _("Generate lights from sector brightness"));
	_generateLights->SetValue(true);
	_generateLights->Bind(wxEVT_CHECKBOX, &WadImportDialog::onGenerateLightsToggled, this);
	grid->Add(_generateLights, 0, wxEXPAND);

	grid->Add(new wxStaticText(this, wxID_ANY, _("Light spacing:")), 0, wxALIGN_CENTER_VERTICAL);

	_lightSpacing = new wxSpinCtrl(this, wxID_ANY);
	_lightSpacing->SetRange(32, 4096);
	_lightSpacing->SetValue(static_cast<int>(defaultLightSpacing));
	grid->Add(_lightSpacing, 0, wxEXPAND);

	mainSizer->Add(grid, 1, wxEXPAND | wxLEFT | wxRIGHT, 10);

	auto* btnSizer = new wxBoxSizer(wxHORIZONTAL);
	btnSizer->AddStretchSpacer();
	btnSizer->Add(new wxButton(this, wxID_CANCEL, _("Cancel")), 0, wxALL, 5);
	btnSizer->Add(new wxButton(this, wxID_OK, _("Import")), 0, wxALL, 5);

	mainSizer->Add(btnSizer, 0, wxEXPAND | wxALL, 5);

	SetSizer(mainSizer);
	Fit();
	CenterOnParent();
}

void WadImportDialog::onGenerateLightsToggled(wxCommandEvent& ev)
{
	_lightSpacing->Enable(_generateLights->GetValue());
}

WadImportDialog::Options WadImportDialog::RunDialog(wxWindow* parent, const std::string& wadPath,
	const std::vector<std::string>& mapNames, double defaultScale, double defaultLightSpacing)
{
	if (mapNames.empty())
	{
		return {};
	}

	WadImportDialog dlg(parent, wadPath, mapNames, defaultScale, defaultLightSpacing);

	Options options;

	if (dlg.ShowModal() != wxID_OK)
	{
		return options;
	}

	auto selection = dlg._mapChoice->GetSelection();

	options.accepted = true;
	options.mapName = mapNames[selection >= 0 ? selection : 0];
	options.scale = dlg._scaleEntry->GetValue();
	options.lightSpacing = dlg._generateLights->GetValue() ? dlg._lightSpacing->GetValue() : 0;

	return options;
}

}
