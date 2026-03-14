#pragma once

#include "icommandsystem.h"
#include "wxutil/dialog/Dialog.h"
#include "wxutil/XmlResourceBasedWidget.h"

namespace ui
{

class SweepDialog : public wxutil::Dialog, private wxutil::XmlResourceBasedWidget
{
public:
    SweepDialog();

    int getSegments();

    static void Show(const cmd::ArgumentList& args);
};

} // namespace ui
