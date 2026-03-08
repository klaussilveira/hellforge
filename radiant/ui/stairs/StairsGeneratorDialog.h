#pragma once

#include "icommandsystem.h"
#include "wxutil/dialog/Dialog.h"
#include "wxutil/XmlResourceBasedWidget.h"

namespace ui
{

class StairsGeneratorDialog : public wxutil::Dialog, private wxutil::XmlResourceBasedWidget
{
private:
    wxWindow* _spiralPanel;
    wxWindow* _turnPanel;
    wxWindow* _landingPanel;

public:
    StairsGeneratorDialog();

    int getStepCount();
    float getStepHeight();
    float getStepDepth();
    float getWidth();
    int getDirection();
    int getType();
    bool getSolid();
    std::string getMaterial();

    // Spiral params
    float getInnerRadius();
    float getOuterRadius();
    float getTotalAngle();

    // L/U params
    int getTurnAt();
    int getTurnDirection();
    float getLandingDepth();

    static void Show(const cmd::ArgumentList& args);

private:
    void onTypeChanged(wxCommandEvent& ev);
    void onBrowseMaterial(wxCommandEvent& ev);
    void updateControlVisibility();
};

} // namespace ui
