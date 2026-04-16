#pragma once

#include "i18n.h"
#include "ui/iusercontrol.h"
#include "TerrainSculptPanel.h"

namespace ui
{

class TerrainSculptControl : public IUserControlCreator
{
public:
    std::string getControlName() override { return UserControl::TerrainSculpt; }
    std::string getDisplayName() override { return _("Terrain Sculpt"); }
    std::string getIcon() override { return "terrain.png"; }

    wxWindow* createWidget(wxWindow* parent) override
    {
        return new TerrainSculptPanel(parent);
    }
};

}
