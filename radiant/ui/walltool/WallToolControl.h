#pragma once

#include "i18n.h"
#include "ui/iusercontrol.h"
#include "WallToolPanel.h"

namespace ui
{

class WallToolControl :
    public IUserControlCreator
{
public:
    std::string getControlName() override
    {
        return UserControl::WallTool;
    }

    std::string getDisplayName() override
    {
        return _("Wall Tool");
    }

    std::string getIcon() override
    {
        return "selection_makeroom.png";
    }

    wxWindow* createWidget(wxWindow* parent) override
    {
        return new WallToolPanel(parent);
    }
};

}
