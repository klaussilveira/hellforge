#pragma once

#include "icommandsystem.h"
#include "wxutil/dialog/Dialog.h"
#include "wxutil/XmlResourceBasedWidget.h"
#include "CableGeometry.h"

#include <vector>

class wxChoice;

namespace ui
{

class CableGeneratorDialog : public wxutil::Dialog, private wxutil::XmlResourceBasedWidget
{
private:
    wxWindow* _gravityPanel;
    wxChoice* _presetChoice;

    struct Preset
    {
        std::string name;
        bool readonly;
        cables::CableParams params;
    };
    std::vector<Preset> _presets;

public:
    CableGeneratorDialog();

    int getCount();
    float getDensity();
    float getSpacing();
    float getSpacingVariation();
    int getSpacingSeed();
    float getRadius();
    float getRandomScale();
    float getMinScale();
    float getMaxScale();
    int getLengthResolution();
    int getCircResolution();
    int getCircResVariation();
    bool getCapEnds();
    bool getFixedSubdivisions();
    int getSubdivisionU();
    int getSubdivisionV();
    std::string getMaterial();
    int getSimType();
    float getGravityMin();
    float getGravityMax();
    int getGravitySeed();
    float getGravityMultiplier();

    static void Show(const cmd::ArgumentList& args);

private:
    void loadPresets();
    void populatePresetChoice();
    void applyPreset(const Preset& preset);
    cables::CableParams readParamsFromControls();

    void onPresetSelected(wxCommandEvent& ev);
    void onSavePreset(wxCommandEvent& ev);
    void onDeletePreset(wxCommandEvent& ev);
    void onSimTypeChanged(wxCommandEvent& ev);
    void onFixedSubdivisionsChanged(wxCommandEvent& ev);
    void onBrowseMaterial(wxCommandEvent& ev);
    void onRandomizeSeed(wxCommandEvent& ev);
    void updateControlVisibility();
};

} // namespace ui
