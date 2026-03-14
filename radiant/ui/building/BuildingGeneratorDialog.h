#pragma once

#include "icommandsystem.h"
#include "wxutil/dialog/Dialog.h"
#include "wxutil/XmlResourceBasedWidget.h"

namespace ui
{

class BuildingGeneratorDialog : public wxutil::Dialog, private wxutil::XmlResourceBasedWidget
{
private:
    wxWindow* _dimensionsPanel;
    wxWindow* _floorHeightPanel;
    wxWindow* _windowParamsPanel;
    wxWindow* _windowCountPanel;
    wxWindow* _cornerExtrudePanel;
    wxWindow* _roofHeightPanel;
    wxWindow* _roofBorderPanel;
    bool _hasBrushSelection;

public:
    BuildingGeneratorDialog(bool hasBrushSelection, double defaultFloorHeight);

    int getFloorCount();
    int getFloorHeightMode();
    float getFloorHeight();
    float getWallThickness();
    float getTrimHeight();
    int getWindowMode();
    int getWindowsPerFloor();
    float getWindowWidth();
    float getWindowHeight();
    float getSillHeight();
    bool getCornerColumns();
    float getCornerExtrude();
    int getRoofType();
    float getRoofHeight();
    float getRoofBorderHeight();
    float getBuildingWidth();
    float getBuildingDepth();
    float getBuildingHeight();
    std::string getWallMaterial();
    std::string getTrimMaterial();

    static void Show(const cmd::ArgumentList& args);

private:
    void onFloorHeightModeChanged(wxCommandEvent& ev);
    void onWindowModeChanged(wxCommandEvent& ev);
    void onRoofTypeChanged(wxCommandEvent& ev);
    void onCornerColumnsChanged(wxCommandEvent& ev);
    void onBrowseWallMaterial(wxCommandEvent& ev);
    void onBrowseTrimMaterial(wxCommandEvent& ev);
    void updateControlVisibility();
};

} // namespace ui
