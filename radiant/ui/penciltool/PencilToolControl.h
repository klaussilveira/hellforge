#pragma once

#include "i18n.h"
#include "ui/iusercontrol.h"
#include "PencilToolPanel.h"

namespace ui
{

class PencilToolControl :
    public IUserControlCreator
{
public:
    std::string getControlName() override
    {
        return UserControl::PencilTool;
    }

    std::string getDisplayName() override
    {
        return _("Pencil Tool");
    }

    std::string getIcon() override
    {
        return "curve_create.png";
    }

    wxWindow* createWidget(wxWindow* parent) override
    {
        return new PencilToolPanel(parent);
    }
};

}
