#pragma once

#include <wx/colour.h>
#include <wx/window.h>

namespace wxutil
{

class UIThemeManager
{
public:
    struct ThemeColours
    {
        wxColour windowBackground;
        wxColour panelBackground;
        wxColour inputBackground;
        wxColour widgetBackground;

        wxColour headerBackground;
        wxColour tabActive;
        wxColour tabInactive;
        wxColour tabBackground;

        wxColour textPrimary;
        wxColour textSecondary;
        wxColour textDisabled;

        wxColour selection;
        wxColour selectionActive;
        wxColour hover;

        wxColour border;
        wxColour borderLight;
        wxColour outline;

        wxColour success;
        wxColour warning;
        wxColour error;
    };

    static UIThemeManager& Instance();

    const ThemeColours& getColours() const;

    void applyTheme(wxWindow* window);
    void applyThemeToWindow(wxWindow* window);

    bool isDarkThemeEnabled() const;
    void setDarkThemeEnabled(bool enabled);

    std::string getIconThemeFolder() const
    {
        return _darkThemeEnabled ? "dark/" : "light/";
    }

private:
    UIThemeManager();
    ~UIThemeManager() = default;

    UIThemeManager(const UIThemeManager&) = delete;
    UIThemeManager& operator=(const UIThemeManager&) = delete;

    void initializeTheme();
    void initDarkModeApi();
    void applyDarkModeToTopLevel(wxWindow* window);
    void applyDarkModeToControl(wxWindow* window);
    void applyThemeRecursive(wxWindow* window);

    ThemeColours _colours;
    bool _darkThemeEnabled;
    bool _darkModeApiAvailable;
    bool _eventsBound;
};

inline UIThemeManager& GlobalUIThemeManager()
{
    return UIThemeManager::Instance();
}

} // namespace wxutil
