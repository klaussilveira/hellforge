#pragma once

#include <wx/dataview.h>
#include <wx/settings.h>
#include "../UIThemeManager.h"

// Background colour needs support by wxWidgets 3.1.1 or 3.1.4 on platforms other than MSW
#if defined(__WXMSW__) || \
    defined(__APPLE__) && wxCHECK_VERSION(3, 1, 4) || \
    defined(__WXGTK__) && wxCHECK_VERSION(3, 1, 1)
#define ATTR_SUPPORTS_BACKGROUND (true)
#else
#define ATTR_SUPPORTS_BACKGROUND (false)
#endif

namespace wxutil
{

class TreeViewItemStyle
{
public:
    // Get the item format for favourites / non-favourites item in a declaration tree
    static wxDataViewItemAttr Declaration(bool isFavourite)
    {
        if (isFavourite)
        {
            wxDataViewItemAttr blueBold;
            // Use theme selection colour for favourites in dark mode
            const auto& colours = GlobalUIThemeManager().getColours();
            blueBold.SetColour(colours.selection);
            blueBold.SetBold(true);

            return blueBold;
        }

        return wxDataViewItemAttr();
    }

    static wxDataViewItemAttr Modified(bool isModified)
    {
        if (isModified)
        {
            wxDataViewItemAttr bold;
            bold.SetBold(true);

            return bold;
        }

        return wxDataViewItemAttr();
    }

    static wxDataViewItemAttr Inherited()
    {
        wxDataViewItemAttr result;
        result.SetColour(wxSystemSettings::GetColour(wxSYS_COLOUR_GRAYTEXT));
        result.SetItalic(true);

        return result;
    }

    static wxDataViewItemAttr ActiveItemStyle()
    {
        wxDataViewItemAttr bold;
        bold.SetBold(true);

        return bold;
    }

    // Styles used by the merge action visualisation in data views

    static void ApplyKeyValueAddedStyle(wxDataViewItemAttr& attr)
    {
        const auto& colours = GlobalUIThemeManager().getColours();
        if (SupportsBackgroundColour())
        {
            // Darker green background for dark theme
            SetBackgroundColour(attr, wxColour(40, 80, 40));
        }
        else
        {
            attr.SetColour(colours.success);
            attr.SetBold(true);
        }
    }

    static void ApplyKeyValueChangedStyle(wxDataViewItemAttr& attr)
    {
        const auto& colours = GlobalUIThemeManager().getColours();
        if (SupportsBackgroundColour())
        {
            // Darker blue background for dark theme
            SetBackgroundColour(attr, wxColour(50, 80, 120));
        }
        else
        {
            attr.SetColour(colours.selection);
            attr.SetBold(true);
        }
    }

    static void ApplyKeyValueRemovedStyle(wxDataViewItemAttr& attr)
    {
        if (SupportsBackgroundColour())
        {
            // Darker red background for dark theme
            SetBackgroundColour(attr, wxColour(100, 50, 50));
        }
        else
        {
            attr.SetColour(wxColour(200, 80, 80));
            attr.SetBold(true);
        }

        SetStrikethrough(attr, true);
    }

    static void ApplyKeyValueAmbiguousStyle(wxDataViewItemAttr& attr)
    {
        const auto& colours = GlobalUIThemeManager().getColours();
        attr.SetColour(colours.textDisabled);
        SetItalic(attr, true);
    }

    static void ApplyKeyValueConflictStyle(wxDataViewItemAttr& attr)
    {
        const auto& colours = GlobalUIThemeManager().getColours();
        if (SupportsBackgroundColour())
        {
            // Darker orange background for dark theme
            SetBackgroundColour(attr, wxColour(100, 60, 20));
        }
        else
        {
            attr.SetColour(colours.warning);
            attr.SetBold(true);
        }
    }

    static void SetStrikethrough(wxDataViewItemAttr& attr, bool enabled)
    {
#if wxCHECK_VERSION(3, 1, 2)
        attr.SetStrikethrough(enabled);
#endif
    }

    static void SetItalic(wxDataViewItemAttr& attr, bool enabled)
    {
#if wxCHECK_VERSION(3, 1, 2)
        attr.SetItalic(enabled);
#endif
    }

private:

    // Returns true if wxWidgets supports setting the background colour of items
    static bool SupportsBackgroundColour()
    {
        return ATTR_SUPPORTS_BACKGROUND;
    }

    static void SetBackgroundColour(wxDataViewItemAttr& attr, const wxColour& colour)
    {
#if ATTR_SUPPORTS_BACKGROUND
        attr.SetBackgroundColour(colour);
#endif
    }
};

}
