#pragma once

#include <sigc++/trackable.h>
#include "wxutil/DockablePanel.h"
#include "imap.h"
#include "inode.h"

#include <memory>
#include <vector>
#include <wx/spinctrl.h>

class wxChoice;
class wxStaticText;
class ISelectable;

namespace ui
{

class TerrainSculptPreview;

class TerrainSculptPanel :
    public wxutil::DockablePanel,
    public sigc::trackable
{
private:
    wxChoice* _targetChoice;
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

    std::vector<scene::INodeWeakPtr> _targets;

    std::unique_ptr<TerrainSculptPreview> _preview;
    bool _previewAttached = false;

    sigc::connection _mapEventConnection;
    sigc::connection _selectionChangedConnection;
    sigc::connection _settingsChangedConnection;

public:
    TerrainSculptPanel(wxWindow* parent);
    ~TerrainSculptPanel() override;

protected:
    void onPanelActivated() override;
    void onPanelDeactivated() override;

private:
    void populateWindow();
    void refreshTargets();
    void pullFromSettings();
    void pushToSettings();
    void updateModeVisibility();
    void onTargetChanged(wxCommandEvent& ev);
    void onChoiceChange(wxCommandEvent& ev);
    void onSpinChange(wxSpinDoubleEvent& ev);
    void onSeedChange(wxSpinEvent& ev);
    void onMapEvent(IMap::MapEvent ev);
    void onSelectionChanged(const ISelectable& selectable);
    void setTarget(const scene::INodePtr& node);
};

}
