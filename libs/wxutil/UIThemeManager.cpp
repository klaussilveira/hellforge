#include "UIThemeManager.h"

#include <wx/app.h>

#ifdef WIN32
#include <wx/msw/private.h>
#include <dwmapi.h>
#include <uxtheme.h>

#ifndef DWMWA_USE_IMMERSIVE_DARK_MODE
#define DWMWA_USE_IMMERSIVE_DARK_MODE 20
#endif

#include <wx/textctrl.h>
#include <wx/listctrl.h>
#include <wx/treectrl.h>
#include <wx/dataview.h>
#include <wx/aui/auibook.h>
#include <wx/toolbar.h>

namespace
{

enum AppMode
{
    AppMode_Default = 0,
    AppMode_AllowDark = 1,
    AppMode_ForceDark = 2,
    AppMode_ForceLight = 3
};

using fnSetPreferredAppMode = DWORD(WINAPI*)(DWORD mode);
using fnAllowDarkModeForWindow = bool(WINAPI*)(HWND hwnd, bool allow);
using fnShouldAppsUseDarkMode = bool(WINAPI*)();
using fnRefreshImmersiveColorPolicyState = void(WINAPI*)();
using fnFlushMenuThemes = void(WINAPI*)();

fnSetPreferredAppMode pSetPreferredAppMode = nullptr;
fnAllowDarkModeForWindow pAllowDarkModeForWindow = nullptr;
fnShouldAppsUseDarkMode pShouldAppsUseDarkMode = nullptr;
fnRefreshImmersiveColorPolicyState pRefreshImmersiveColorPolicyState = nullptr;
fnFlushMenuThemes pFlushMenuThemes = nullptr;

bool isNativeControl(wxWindow* window)
{
    return dynamic_cast<wxTreeCtrl*>(window) ||
           dynamic_cast<wxListCtrl*>(window) ||
           dynamic_cast<wxDataViewCtrl*>(window);
}

void applyAuiNotebookTheme(wxAuiNotebook* notebook, const wxutil::UIThemeManager::ThemeColours& colours)
{
    auto* art = notebook->GetArtProvider();
    if (!art)
        return;

    art->SetColour(colours.panelBackground);
    art->SetActiveColour(colours.tabActive);

    notebook->SetBackgroundColour(colours.windowBackground);
    notebook->SetForegroundColour(colours.textPrimary);
}

}

#endif // WIN32

namespace wxutil
{

UIThemeManager& UIThemeManager::Instance()
{
    static UIThemeManager instance;
    return instance;
}

UIThemeManager::UIThemeManager()
    : _darkThemeEnabled(true),
      _darkModeApiAvailable(false),
      _eventsBound(false)
{
    initializeTheme();
    initDarkModeApi();
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

void UIThemeManager::initDarkModeApi()
{
#ifdef WIN32
    HMODULE hUxTheme = LoadLibraryW(L"uxtheme.dll");
    if (!hUxTheme)
        return;

    pSetPreferredAppMode = reinterpret_cast<fnSetPreferredAppMode>(
        GetProcAddress(hUxTheme, MAKEINTRESOURCEA(135)));
    pAllowDarkModeForWindow = reinterpret_cast<fnAllowDarkModeForWindow>(
        GetProcAddress(hUxTheme, MAKEINTRESOURCEA(133)));
    pShouldAppsUseDarkMode = reinterpret_cast<fnShouldAppsUseDarkMode>(
        GetProcAddress(hUxTheme, MAKEINTRESOURCEA(132)));

    pRefreshImmersiveColorPolicyState = reinterpret_cast<fnRefreshImmersiveColorPolicyState>(
        GetProcAddress(hUxTheme, MAKEINTRESOURCEA(104)));
    pFlushMenuThemes = reinterpret_cast<fnFlushMenuThemes>(
        GetProcAddress(hUxTheme, MAKEINTRESOURCEA(136)));

    if (pSetPreferredAppMode && pAllowDarkModeForWindow)
    {
        _darkModeApiAvailable = true;

        if (_darkThemeEnabled)
        {
            pSetPreferredAppMode(AppMode_ForceDark);

            if (pRefreshImmersiveColorPolicyState)
                pRefreshImmersiveColorPolicyState();
            if (pFlushMenuThemes)
                pFlushMenuThemes();
        }
    }
#endif
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

#ifdef WIN32
    if (_darkModeApiAvailable && pSetPreferredAppMode)
    {
        pSetPreferredAppMode(enabled ? AppMode_ForceDark : AppMode_Default);

        if (pRefreshImmersiveColorPolicyState)
            pRefreshImmersiveColorPolicyState();
        if (pFlushMenuThemes)
            pFlushMenuThemes();
    }

    if (_darkModeApiAvailable && !_eventsBound && wxTheApp)
    {
        _eventsBound = true;

        wxTheApp->Bind(wxEVT_CREATE, [this](wxWindowCreateEvent& ev) {
            ev.Skip();

            if (!_darkThemeEnabled)
                return;

            auto* window = ev.GetWindow();
            if (!window)
                return;

            if (window->IsTopLevel())
                applyDarkModeToTopLevel(window);

            applyThemeToWindow(window);
        });
    }
#endif
}

void UIThemeManager::applyTheme(wxWindow* window)
{
    if (!window || !_darkThemeEnabled)
        return;

    applyDarkModeToTopLevel(window);
    applyThemeRecursive(window);
}

void UIThemeManager::applyDarkModeToTopLevel(wxWindow* window)
{
#ifdef WIN32
    if (!window->IsTopLevel())
        return;

    HWND hwnd = static_cast<HWND>(window->GetHandle());
    if (!hwnd)
        return;

    BOOL darkMode = TRUE;
    DwmSetWindowAttribute(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE,
                          &darkMode, sizeof(darkMode));

    if (_darkModeApiAvailable && pAllowDarkModeForWindow)
        pAllowDarkModeForWindow(hwnd, true);

    if (pFlushMenuThemes)
        pFlushMenuThemes();

    DrawMenuBar(hwnd);
#endif
}

void UIThemeManager::applyDarkModeToControl(wxWindow* window)
{
#ifdef WIN32
    HWND hwnd = static_cast<HWND>(window->GetHandle());
    if (!hwnd)
        return;

    if (_darkModeApiAvailable && pAllowDarkModeForWindow)
        pAllowDarkModeForWindow(hwnd, true);

    if (isNativeControl(window))
        SetWindowTheme(hwnd, L"Explorer", nullptr);
    else
        SetWindowTheme(hwnd, L"DarkMode_Explorer", nullptr);

    SendMessageW(hwnd, WM_THEMECHANGED, 0, 0);
#endif
}

void UIThemeManager::applyThemeToWindow(wxWindow* window)
{
    if (!window || !_darkThemeEnabled)
        return;

#ifdef WIN32
    applyDarkModeToControl(window);

    if (auto* notebook = dynamic_cast<wxAuiNotebook*>(window))
    {
        applyAuiNotebookTheme(notebook, _colours);
        return;
    }

    if (auto* textCtrl = dynamic_cast<wxTextCtrl*>(window))
    {
        textCtrl->SetBackgroundColour(_colours.inputBackground);
        textCtrl->SetForegroundColour(_colours.textPrimary);
        return;
    }

    if (auto* toolbar = dynamic_cast<wxToolBar*>(window))
    {
        toolbar->SetBackgroundColour(_colours.windowBackground);
        toolbar->SetForegroundColour(_colours.textPrimary);
        return;
    }

    if (!isNativeControl(window))
    {
        window->SetBackgroundColour(_colours.panelBackground);
        window->SetForegroundColour(_colours.textPrimary);
    }
#endif
}

void UIThemeManager::applyThemeRecursive(wxWindow* window)
{
    if (!window)
        return;

    applyThemeToWindow(window);

    wxWindowList& children = window->GetChildren();
    for (auto it = children.begin(); it != children.end(); ++it)
    {
        applyThemeRecursive(*it);
    }
}

} // namespace wxutil
