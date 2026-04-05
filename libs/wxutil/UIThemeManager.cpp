#include "UIThemeManager.h"

namespace wxutil
{

UIThemeManager& UIThemeManager::Instance()
{
    static UIThemeManager instance;
    return instance;
}

UIThemeManager::UIThemeManager()
    : _darkThemeEnabled(false),
      _darkModeApiAvailable(false),
      _eventsBound(false)
{
    initializeTheme();
}

void UIThemeManager::initializeTheme()
{
    // Blender 4 Dark Theme colour values
    // Based on userdef_default_theme.c from Blender source

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
#ifdef WIN32
    _darkThemeEnabled = false;
#else
    _darkThemeEnabled = enabled;
#endif
}

void UIThemeManager::applyTheme(wxWindow*) {}
void UIThemeManager::applyThemeToWindow(wxWindow*) {}
void UIThemeManager::initDarkModeApi() {}
void UIThemeManager::applyDarkModeToTopLevel(wxWindow*) {}
void UIThemeManager::applyDarkModeToControl(wxWindow*) {}
void UIThemeManager::applyThemeRecursive(wxWindow*) {}

} // namespace wxutil
