#pragma once

#include "AasFileControl.h"
#include "imap.h"
#include <sigc++/connection.h>

#include "wxutil/DockablePanel.h"

class wxFlexGridSizer;
class wxButton;
class wxStaticText;

namespace ui
{

class AasVisualisationPanel :
	public wxutil::DockablePanel
{
private:
    std::vector<AasFileControlPtr> _aasControls;

	wxPanel* _dialogPanel;

	wxFlexGridSizer* _controlContainer;
    wxButton* _rescanButton;
    wxButton* _loadFileButton;
    wxStaticText* _statusLabel;

	sigc::connection _mapEventSlot;

public:
	AasVisualisationPanel(wxWindow* parent);
    ~AasVisualisationPanel() override;

protected:
    void onPanelActivated() override;
    void onPanelDeactivated() override;

private:
    void connectListeners();
    void disconnectListeners();

	void refresh();
	void update();

	void populateWindow();
    void createButtons();
	void clearControls();

    void onLoadFile();
    void loadAasFileFromPath(const std::string& absolutePath);

	void onMapEvent(IMap::MapEvent ev);
};

}
