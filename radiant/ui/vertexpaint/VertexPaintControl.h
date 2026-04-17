#pragma once

#include "i18n.h"
#include "ui/iusercontrol.h"
#include "VertexPaintPanel.h"

namespace ui
{

class VertexPaintControl : public IUserControlCreator
{
public:
    std::string getControlName() override { return UserControl::VertexPaint; }
    std::string getDisplayName() override { return _("Vertex Paint"); }
    std::string getIcon() override { return "vertexpaint.png"; }

    wxWindow* createWidget(wxWindow* parent) override
    {
        return new VertexPaintPanel(parent);
    }
};

}
