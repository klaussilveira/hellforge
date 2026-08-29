#pragma once

#include "i18n.h"
#include "Bitmap.h"

#include <wx/bmpbuttn.h>
#include <wx/button.h>
#include <wx/sizer.h>

namespace wxutil
{

constexpr const char* const PICKER_ICON = "pick.png";

inline wxBitmapButton* PickerButton(wxWindow* parent, const wxString& tooltip = wxEmptyString)
{
    auto* button = new wxBitmapButton(parent, wxID_ANY, GetLocalBitmap(PICKER_ICON));
    button->SetToolTip(tooltip.IsEmpty() ? _("Choose material") : tooltip);

    return button;
}

inline void ApplyPickerIcon(wxButton* button, const wxString& tooltip = wxEmptyString)
{
    button->SetBitmap(GetLocalBitmap(PICKER_ICON));

    if (!tooltip.IsEmpty())
    {
        button->SetToolTip(tooltip);
    }
    else if (button->GetLabel().IsEmpty() && button->GetToolTipText().IsEmpty())
    {
        button->SetToolTip(_("Choose material"));
    }
}

inline wxButton* ReplaceWithPickerButton(wxButton* oldButton,
                                        const wxString& tooltip = wxEmptyString)
{
    auto* sizer = oldButton->GetContainingSizer();
    if (sizer == nullptr)
    {
        ApplyPickerIcon(oldButton);
        return oldButton;
    }

    auto existingTooltip = oldButton->GetToolTipText();
    auto* newButton = PickerButton(oldButton->GetParent(),
        !tooltip.IsEmpty() ? tooltip : existingTooltip);

    newButton->SetName(oldButton->GetName());
    newButton->Enable(oldButton->IsEnabled());

    sizer->Replace(oldButton, newButton);

    oldButton->SetName(wxEmptyString);
    oldButton->Destroy();

    sizer->Layout();

    return newButton;
}

}
