#pragma once

#include <sigc++/trackable.h>
#include "wxutil/DockablePanel.h"

#include <memory>
#include <wx/spinctrl.h>

class wxChoice;
class wxStaticText;

namespace ui
{

class TerrainSculptPreview;

class TerrainSculptPanel :
    public wxutil::DockablePanel,
    public sigc::trackable
{
private:
    wxChoice* _modeChoice;
    wxChoice* _brushChoice;
    wxChoice* _falloffTypeChoice;
    wxSpinCtrlDouble* _radius;
    wxSpinCtrlDouble* _strength;
    wxSpinCtrlDouble* _falloff;
    wxSpinCtrlDouble* _filterRadius;
    wxSpinCtrlDouble* _flattenHeight;
    wxChoice* _noiseAlgorithm;
    wxSpinCtrlDouble* _noiseScale;
    wxSpinCtrlDouble* _noiseAmount;
    wxSpinCtrl* _noiseSeed;

    wxStaticText* _filterRadiusLabel;
    wxStaticText* _flattenHeightLabel;
    wxStaticText* _noiseAlgorithmLabel;
    wxStaticText* _noiseScaleLabel;
    wxStaticText* _noiseAmountLabel;
    wxStaticText* _noiseSeedLabel;

    std::unique_ptr<TerrainSculptPreview> _preview;
    bool _previewAttached = false;

    sigc::connection _settingsChangedConnection;

public:
    TerrainSculptPanel(wxWindow* parent);
    ~TerrainSculptPanel() override;

protected:
    void onPanelActivated() override;
    void onPanelDeactivated() override;

private:
    void populateWindow();
    void pullFromSettings();
    void pushToSettings();
    void updateModeVisibility();
    void onChoiceChange(wxCommandEvent& ev);
    void onSpinChange(wxSpinDoubleEvent& ev);
    void onSeedChange(wxSpinEvent& ev);
};

}
