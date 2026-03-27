#pragma once

#include "icommandsystem.h"
#include "wxutil/dialog/DialogBase.h"
#include "GameSetupPage.h"
#include "GameConfigUtil.h"
#include "messages/GameConfigNeededMessage.h"

#include <map>

class wxChoicebook;
class wxBookCtrlEvent;
class wxChoice;

namespace ui
{

class GameSetupDialog :
	public wxutil::DialogBase
{
private:
	wxChoicebook* _book;
	wxChoice* _savedConfigsChoice;

	std::map<std::string, game::GameConfiguration> _savedConfigs;

	GameSetupDialog(wxWindow* parent);

public:
	std::string getSelectedGameType();

	static void Show(const cmd::ArgumentList& args);

	static void HandleGameConfigMessage(game::ConfigurationNeeded& message);

private:
	static void TryGetConfig(const std::function<void(const game::GameConfiguration&)>& onSuccess);

	GameSetupPage* getSelectedPage();
	GameSetupPage* getPage(int num);
	void setSelectedPage(const std::string& name);

	void initialiseControls();
	void populateSavedConfigs();

	void onSave(wxCommandEvent& ev);
	void onCancel(wxCommandEvent& ev);
	void onPageChanged(wxBookCtrlEvent& ev);
	void onSavedConfigSelected(wxCommandEvent& ev);
	void onSaveConfig(wxCommandEvent& ev);
	void onDeleteConfig(wxCommandEvent& ev);

	void tryEndModal(wxStandardID result);
};

}
