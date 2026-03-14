#pragma once

#include "icommandsystem.h"
#include "wxutil/dialog/Dialog.h"
#include "wxutil/XmlResourceBasedWidget.h"

namespace ui
{

class CornerPatchDialog : public wxutil::Dialog, private wxutil::XmlResourceBasedWidget
{
public:
    CornerPatchDialog();

    void setRadius(int value);

    int getSegments();
    float getRadius();
    float getArcDegrees();
    bool getInvert();

    static void Show(const cmd::ArgumentList& args);
};

} // namespace ui
