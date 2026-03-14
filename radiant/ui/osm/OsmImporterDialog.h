#pragma once

#include "icommandsystem.h"
#include "wxutil/dialog/Dialog.h"
#include "wxutil/XmlResourceBasedWidget.h"

namespace ui
{

class OsmImporterDialog : public wxutil::Dialog, private wxutil::XmlResourceBasedWidget
{
public:
    OsmImporterDialog();

    std::string getFilePath();
    float getUnitsPerMeter();
    float getLevelHeight();
    float getBaseZ();
    std::string getWallMaterial();
    std::string getRoofMaterial();
    std::string getFloorMaterial();
    std::string getRoadMaterial();
    std::string getSidewalkMaterial();
    float getDefaultLaneWidth();
    float getSidewalkWidth();
    float getCurbHeight();

    static void Show(const cmd::ArgumentList& args);

private:
    void onBrowseFile(wxCommandEvent& ev);
    void onBrowseWallMaterial(wxCommandEvent& ev);
    void onBrowseRoofMaterial(wxCommandEvent& ev);
    void onBrowseFloorMaterial(wxCommandEvent& ev);
    void onBrowseRoadMaterial(wxCommandEvent& ev);
    void onBrowseSidewalkMaterial(wxCommandEvent& ev);
    void browseMaterial(const char* controlName);
};

} // namespace ui
