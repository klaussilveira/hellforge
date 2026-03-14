#pragma once

#include "icommandsystem.h"
#include "wxutil/dialog/Dialog.h"
#include "wxutil/XmlResourceBasedWidget.h"

namespace ui
{

class ArchGeneratorDialog : public wxutil::Dialog, private wxutil::XmlResourceBasedWidget
{
public:
    ArchGeneratorDialog();

    int getSegments();
    float getInnerRadius();
    float getWallThickness();
    float getDepth();
    float getLength();
    float getArcDegrees();
    float getStartAngle();
    std::string getMaterial();

    static void Show(const cmd::ArgumentList& args);

private:
    void onBrowseMaterial(wxCommandEvent& ev);
};

} // namespace ui
