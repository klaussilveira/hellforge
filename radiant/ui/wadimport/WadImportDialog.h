#pragma once

#include "wxutil/dialog/DialogBase.h"

#include <string>
#include <vector>

class wxChoice;
class wxCheckBox;
class wxSpinCtrlDouble;
class wxSpinCtrl;

namespace ui
{

class WadImportDialog :
	public wxutil::DialogBase
{
public:
	struct Options
	{
		bool accepted = false;
		std::string mapName;
		double scale = 1.0;
		double lightSpacing = 0;
	};

	static Options RunDialog(wxWindow* parent, const std::string& wadPath,
		const std::vector<std::string>& mapNames, double defaultScale, double defaultLightSpacing);

private:
	wxChoice* _mapChoice;
	wxSpinCtrlDouble* _scaleEntry;
	wxCheckBox* _generateLights;
	wxSpinCtrl* _lightSpacing;

	WadImportDialog(wxWindow* parent, const std::string& wadPath,
		const std::vector<std::string>& mapNames, double defaultScale, double defaultLightSpacing);

	void onGenerateLightsToggled(wxCommandEvent& ev);
};

}
