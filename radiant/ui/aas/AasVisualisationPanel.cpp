#include "AasVisualisationPanel.h"

#include "i18n.h"
#include "iaasfile.h"
#include "imap.h"
#include "itextstream.h"

#include <wx/button.h>
#include <wx/tglbtn.h>
#include <wx/sizer.h>
#include <wx/panel.h>
#include <wx/scrolwin.h>
#include <wx/stattext.h>
#include <wx/filedlg.h>

#include "registry/Widgets.h"

namespace ui
{

AasVisualisationPanel::AasVisualisationPanel(wxWindow* parent) :
    DockablePanel(parent),
	_dialogPanel(nullptr),
	_controlContainer(nullptr),
    _loadFileButton(nullptr),
    _statusLabel(nullptr)
{
	populateWindow();

    SetMinClientSize(wxSize(135, 100));
}

AasVisualisationPanel::~AasVisualisationPanel()
{
    if (panelIsActive())
    {
        disconnectListeners();
    }
}

void AasVisualisationPanel::onPanelActivated()
{
    connectListeners();
    refresh();
}

void AasVisualisationPanel::onPanelDeactivated()
{
    disconnectListeners();
}

void AasVisualisationPanel::connectListeners()
{
    _mapEventSlot = GlobalMapModule().signal_mapEvent().connect(
        sigc::mem_fun(*this, &AasVisualisationPanel::onMapEvent));
}

void AasVisualisationPanel::disconnectListeners()
{
    _mapEventSlot.disconnect();
}

void AasVisualisationPanel::onMapEvent(IMap::MapEvent ev)
{
	switch (ev)
	{
	case IMap::MapEvent::MapLoaded:
		refresh();
		break;
	case IMap::MapEvent::MapUnloading:
		clearControls();
		break;
	default:
		break;
	};
}

void AasVisualisationPanel::populateWindow()
{
    auto scrollView = new wxScrolledWindow(this, wxID_ANY);
    scrollView->SetScrollRate(0, 15);

    _dialogPanel = scrollView;

    _dialogPanel->SetSizer(new wxBoxSizer(wxVERTICAL));

    _controlContainer = new wxFlexGridSizer(1, 2, 3, 3);
    _controlContainer->AddGrowableCol(0);

    _dialogPanel->GetSizer()->Add(_controlContainer, 1, wxEXPAND | wxTOP | wxLEFT | wxRIGHT, 12);

    createButtons();

    _dialogPanel->FitInside();

    SetSizer(new wxBoxSizer(wxVERTICAL));
    GetSizer()->Add(scrollView, 1, wxEXPAND);
}

void AasVisualisationPanel::createButtons()
{
	wxToggleButton* showNumbersButton = new wxToggleButton(_dialogPanel, wxID_ANY, _("Show Area Numbers"));
	registry::bindWidget(showNumbersButton, map::RKEY_SHOW_AAS_AREA_NUMBERS);

	wxToggleButton* hideDistantAreasButton = new wxToggleButton(_dialogPanel, wxID_ANY, _("Hide distant Areas"));
	registry::bindWidget(hideDistantAreasButton, map::RKEY_HIDE_DISTANT_AAS_AREAS);

	_rescanButton = new wxButton(_dialogPanel, wxID_ANY, _("Search for AAS Files"));
    _rescanButton->Bind(wxEVT_BUTTON, [this](wxCommandEvent& ev) { refresh(); });

    _loadFileButton = new wxButton(_dialogPanel, wxID_ANY, _("Load AAS File..."));
    _loadFileButton->Bind(wxEVT_BUTTON, [this](wxCommandEvent& ev) { onLoadFile(); });

    _statusLabel = new wxStaticText(_dialogPanel, wxID_ANY, "",
        wxDefaultPosition, wxDefaultSize, wxST_NO_AUTORESIZE);
    _statusLabel->SetForegroundColour(wxColour(180, 180, 180));
    _statusLabel->Wrap(250);

	_dialogPanel->GetSizer()->Add(showNumbersButton, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, 12);
	_dialogPanel->GetSizer()->Add(hideDistantAreasButton, 0, wxEXPAND | wxLEFT | wxRIGHT, 12);
	_dialogPanel->GetSizer()->Add(_rescanButton, 0, wxEXPAND | wxLEFT | wxRIGHT, 12);
    _dialogPanel->GetSizer()->Add(_loadFileButton, 0, wxEXPAND | wxLEFT | wxRIGHT, 12);
    _dialogPanel->GetSizer()->Add(_statusLabel, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 12);
}

void AasVisualisationPanel::clearControls()
{
	_aasControls.clear();
	_controlContainer->Clear(true);
}

void AasVisualisationPanel::refresh()
{
	clearControls();

	std::map<std::string, AasFileControlPtr> sortedControls;

    std::string mapName = GlobalMapModule().getMapName();

    std::list<map::AasFileInfo> aasFiles = GlobalAasFileManager().getAasFilesForMap(mapName);

    std::string statusText;

    if (aasFiles.empty())
    {
        statusText = "No AAS files found.\nMap: " + mapName;

        auto types = GlobalAasFileManager().getAasTypes();
        if (types.empty())
        {
            statusText += "\nNo AAS types defined (missing aas_types entityDef?)";
        }
        else
        {
            statusText += "\nSearched for extensions:";
            for (const auto& type : types)
            {
                statusText += " ." + type.fileExtension;
            }
        }
    }
    else
    {
        statusText = "Found " + std::to_string(aasFiles.size()) + " AAS file(s):";
        for (const auto& info : aasFiles)
        {
            statusText += "\n" + info.absolutePath;
        }
    }

    rMessage() << "[AAS] " << statusText << std::endl;

    if (_statusLabel)
    {
        _statusLabel->SetLabel(statusText);
        _statusLabel->Wrap(250);
    }

    for (map::AasFileInfo& info : aasFiles)
    {
        sortedControls[info.type.fileExtension] = std::make_shared<AasFileControl>(_dialogPanel, info);
    }

    for (const auto& pair : sortedControls)
    {
        _aasControls.push_back(pair.second);
    }

	_controlContainer->SetRows(static_cast<int>(_aasControls.size()));

	for (auto i = _aasControls.begin(); i != _aasControls.end(); ++i)
	{
		_controlContainer->Add((*i)->getToggle(), 1, wxEXPAND);
		_controlContainer->Add((*i)->getButtons(), 0, wxEXPAND);

        if (i == _aasControls.begin())
        {
            (*i)->getToggle()->SetFocus();
        }
	}

	_controlContainer->Layout();
	_dialogPanel->FitInside();

	update();
}

void AasVisualisationPanel::update()
{
	for (const auto& control : _aasControls)
	{
		control->update();
	}
}

void AasVisualisationPanel::onLoadFile()
{
    wxFileDialog dialog(this, _("Select AAS File"), "", "",
        "AAS files (*.aas*)|*.aas*|All files (*.*)|*.*",
        wxFD_OPEN | wxFD_FILE_MUST_EXIST);

    if (dialog.ShowModal() == wxID_CANCEL)
    {
        return;
    }

    std::string path = dialog.GetPath().ToStdString();
    loadAasFileFromPath(path);
}

void AasVisualisationPanel::loadAasFileFromPath(const std::string& absolutePath)
{
    std::string ext = absolutePath.substr(absolutePath.rfind('.') + 1);

    map::AasFileInfo info;
    info.absolutePath = absolutePath;
    info.type.fileExtension = ext;
    info.type.entityDefName = ext;

    auto control = std::make_shared<AasFileControl>(_dialogPanel, info);

    _aasControls.push_back(control);

    _controlContainer->SetRows(static_cast<int>(_aasControls.size()));
    _controlContainer->Add(control->getToggle(), 1, wxEXPAND);
    _controlContainer->Add(control->getButtons(), 0, wxEXPAND);

    _controlContainer->Layout();
    _dialogPanel->FitInside();

    if (_statusLabel)
    {
        _statusLabel->SetLabel("Loaded: " + absolutePath);
        _statusLabel->Wrap(250);
    }

    rMessage() << "[AAS] Manually loaded: " << absolutePath << std::endl;
}

}
