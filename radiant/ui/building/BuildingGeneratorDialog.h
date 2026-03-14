#pragma once

#include "icommandsystem.h"
#include "wxutil/dialog/Dialog.h"
#include "wxutil/XmlResourceBasedWidget.h"

namespace ui
{

class BuildingGeneratorDialog : public wxutil::Dialog, private wxutil::XmlResourceBasedWidget
{
private:
    wxWindow* _slantedRoofPanel;
    wxWindow* _aRoofPanel;
    wxWindow* _borderTrimPanel;

public:
    BuildingGeneratorDialog();

    // Floor params
    int getFloorCount();
    float getFloorHeight();
    float getFloorThickness();
    float getTrimHeight();
    float getWallThickness();

    // Window params
    int getWindowMode(); // 0=Automatic, 1=Manual count
    int getWindowsPerWall();
    float getWindowWidth();
    float getWindowHeight();
    float getWindowSillHeight();
    float getWindowInset();

    // Roof params
    int getRoofType(); // 0=Flat, 1=Flat+Border, 2=Slanted, 3=A-Frame
    float getRoofBorderHeight();
    float getRoofSlopeHeight();
    int getRoofSlopeDirection(); // 0=East, 1=North, 2=West, 3=South
    float getARoofHeight();
    int getARoofDirection(); // 0=East-West ridge, 1=North-South ridge

    std::string getWallMaterial();
    std::string getTrimMaterial();
    std::string getWindowFrameMaterial();

    static void Show(const cmd::ArgumentList& args);

private:
    void onRoofTypeChanged(wxCommandEvent& ev);
    void onWindowModeChanged(wxCommandEvent& ev);
    void onBrowseWallMaterial(wxCommandEvent& ev);
    void onBrowseTrimMaterial(wxCommandEvent& ev);
    void onBrowseWindowFrameMaterial(wxCommandEvent& ev);
    void updateControlVisibility();
};

} // namespace ui
