#pragma once

#include "i18n.h"
#include "ui/iusercontrol.h"
#include "AssetBrowserPanel.h"

namespace ui
{

class AssetBrowserControl :
    public IUserControlCreator
{
public:
    std::string getControlName() override
    {
        return UserControl::AssetBrowser;
    }

    std::string getDisplayName() override
    {
        return _("Asset Browser");
    }

    std::string getIcon() override
    {
        return "model16green.png";
    }

    wxWindow* createWidget(wxWindow* parent) override
    {
        return new AssetBrowserPanel(parent);
    }
};

}
