#pragma once

#include <wx/colour.h>
#include <wx/window.h>

namespace wxutil
{

/**
 * @brief Manages UI theming for wxWidgets components.
 *
 * Provides a Blender 4-style dark theme with consistent colors across
 * all UI elements. The theme is applied programmatically to wxWidgets
 * windows, dialogs, and controls.
 */
class UIThemeManager
{
public:
    // Blender 4 Dark Theme colour definitions
    struct ThemeColours
    {
        // Main background colours
        wxColour windowBackground;      // Main window background
        wxColour panelBackground;       // Panel/secondary areas
        wxColour inputBackground;       // Text inputs, lists, dark areas
        wxColour widgetBackground;      // Button/widget backgrounds

        // Header and toolbar colours
        wxColour headerBackground;      // Header/toolbar background
        wxColour tabActive;             // Active tab background
        wxColour tabInactive;           // Inactive tab background
        wxColour tabBackground;         // Tab bar background

        // Text colours
        wxColour textPrimary;           // Primary text colour
        wxColour textSecondary;         // Secondary/dimmed text
        wxColour textDisabled;          // Disabled text

        // Selection and accent colours
        wxColour selection;             // Selection highlight
        wxColour selectionActive;       // Active/focused selection
        wxColour hover;                 // Hover state

        // Border and outline colours
        wxColour border;                // Standard border
        wxColour borderLight;           // Subtle borders
        wxColour outline;               // Widget outlines

        // Status colours
        wxColour success;               // Success/added state
        wxColour warning;               // Warning state
        wxColour error;                 // Error state
    };

    /**
     * @brief Get the singleton instance of UIThemeManager.
     */
    static UIThemeManager& Instance();

    /**
     * @brief Get the current theme colours.
     */
    const ThemeColours& getColours() const;

    /**
     * @brief Apply the dark theme to a window and all its children.
     * @param window The window to theme (recursively applies to children).
     */
    void applyTheme(wxWindow* window);

    /**
     * @brief Apply theme colours to a specific window only (non-recursive).
     * @param window The window to theme.
     */
    void applyThemeToWindow(wxWindow* window);

    /**
     * @brief Check if dark theming is enabled.
     */
    bool isDarkThemeEnabled() const;

    /**
     * @brief Enable or disable dark theming.
     */
    void setDarkThemeEnabled(bool enabled);

private:
    UIThemeManager();
    ~UIThemeManager() = default;

    // Non-copyable
    UIThemeManager(const UIThemeManager&) = delete;
    UIThemeManager& operator=(const UIThemeManager&) = delete;

    void initializeTheme();
    void applyThemeRecursive(wxWindow* window);

    ThemeColours _colours;
    bool _darkThemeEnabled;
};

/**
 * @brief Global accessor for the UI theme manager.
 */
inline UIThemeManager& GlobalUIThemeManager()
{
    return UIThemeManager::Instance();
}

} // namespace wxutil
