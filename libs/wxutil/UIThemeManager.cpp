#include "UIThemeManager.h"

#ifdef WIN32
#include <wx/msw/private.h>
#include <dwmapi.h>
#ifndef DWMWA_USE_IMMERSIVE_DARK_MODE
#define DWMWA_USE_IMMERSIVE_DARK_MODE 20
#endif
#endif

namespace wxutil
{

UIThemeManager& UIThemeManager::Instance()
{
    static UIThemeManager instance;
    return instance;
}

UIThemeManager::UIThemeManager()
    : _darkThemeEnabled(true)
{
    initializeTheme();
}

void UIThemeManager::initializeTheme()
{
    // Blender 4 Dark Theme colour values
    // Based on userdef_default_theme.c from Blender source
    // These are used for programmatic theming of specific widgets
    // that don't respond to GTK theme changes

    // Main backgrounds
    _colours.windowBackground = wxColour(48, 48, 48);       // #303030 - main window
    _colours.panelBackground = wxColour(61, 61, 61);        // #3d3d3d - panels
    _colours.inputBackground = wxColour(29, 29, 29);        // #1d1d1d - inputs/lists
    _colours.widgetBackground = wxColour(84, 84, 84);       // #545454 - widgets

    // Headers and tabs
    _colours.headerBackground = wxColour(48, 48, 48);       // #303030 - headers
    _colours.tabActive = wxColour(48, 48, 48);              // #303030 - active tab
    _colours.tabInactive = wxColour(29, 29, 29);            // #1d1d1d - inactive tab
    _colours.tabBackground = wxColour(24, 24, 24);          // #181818 - tab bar

    // Text colours
    _colours.textPrimary = wxColour(230, 230, 230);         // #e6e6e6 - main text
    _colours.textSecondary = wxColour(166, 166, 166);       // #a6a6a6 - secondary text
    _colours.textDisabled = wxColour(128, 128, 128);        // #808080 - disabled text

    // Selection and accent
    _colours.selection = wxColour(71, 114, 179);            // #4772b3 - primary selection
    _colours.selectionActive = wxColour(255, 160, 40);      // #ffa028 - active selection
    _colours.hover = wxColour(96, 96, 96);                  // #606060 - hover state

    // Borders
    _colours.border = wxColour(61, 61, 61);                 // #3d3d3d - standard border
    _colours.borderLight = wxColour(41, 41, 41);            // #292929 - subtle border
    _colours.outline = wxColour(61, 61, 61);                // #3d3d3d - outlines

    // Status colours (brighter for visibility on dark backgrounds)
    _colours.success = wxColour(100, 200, 100);             // Bright green for success
    _colours.warning = wxColour(255, 180, 80);              // Bright orange for warning
    _colours.error = wxColour(255, 100, 100);               // Bright red for error
}

const UIThemeManager::ThemeColours& UIThemeManager::getColours() const
{
    return _colours;
}

bool UIThemeManager::isDarkThemeEnabled() const
{
    return _darkThemeEnabled;
}

void UIThemeManager::setDarkThemeEnabled(bool enabled)
{
    _darkThemeEnabled = enabled;
}

void UIThemeManager::applyTheme(wxWindow* window)
{
    if (!window || !_darkThemeEnabled)
    {
        return;
    }

#ifdef WIN32
    if (window->IsTopLevel())
    {
        HWND hwnd = static_cast<HWND>(window->GetHandle());
        if (hwnd)
        {
            BOOL darkMode = TRUE;
            DwmSetWindowAttribute(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE,
                                  &darkMode, sizeof(darkMode));
        }
    }
#endif

    applyThemeRecursive(window);
}

void UIThemeManager::applyThemeToWindow(wxWindow* window)
{
    if (!window || !_darkThemeEnabled)
    {
        return;
    }

#ifdef WIN32
    window->SetBackgroundColour(_colours.panelBackground);
    window->SetForegroundColour(_colours.textPrimary);
#endif
}

void UIThemeManager::applyThemeRecursive(wxWindow* window)
{
    if (!window)
    {
        return;
    }

    applyThemeToWindow(window);

    // Recurse into children
    wxWindowList& children = window->GetChildren();
    for (wxWindowList::iterator it = children.begin(); it != children.end(); ++it)
    {
        applyThemeRecursive(*it);
    }
}

} // namespace wxutil
