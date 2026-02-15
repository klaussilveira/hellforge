#pragma once

#include <string>
#include <sigc++/trackable.h>
#include <sigc++/signal.h>

namespace ui
{

namespace
{
    const std::string RKEY_RENDERING_QUALITY_ROOT = "user/ui/renderingQuality";

    // Line rendering
    const std::string RKEY_LINE_ANTIALIASING = RKEY_RENDERING_QUALITY_ROOT + "/lineAntialiasing";
    const std::string RKEY_LINE_SMOOTH_HINT = RKEY_RENDERING_QUALITY_ROOT + "/lineSmoothHint";

    // Point rendering
    const std::string RKEY_POINT_ANTIALIASING = RKEY_RENDERING_QUALITY_ROOT + "/pointAntialiasing";

    // Multisampling
    const std::string RKEY_MULTISAMPLE_ENABLED = RKEY_RENDERING_QUALITY_ROOT + "/multisampleEnabled";

    // Point sizes for vertex handles
    const std::string RKEY_VERTEX_POINT_SIZE = RKEY_RENDERING_QUALITY_ROOT + "/vertexPointSize";
    const std::string RKEY_VERTEX_POINT_SMOOTH = RKEY_RENDERING_QUALITY_ROOT + "/vertexPointSmooth";
}

/**
 * Hint level for OpenGL rendering quality.
 * Maps to GL_FASTEST, GL_DONT_CARE, GL_NICEST
 */
enum class RenderHintLevel
{
    Fastest = 0,
    DontCare = 1,
    Nicest = 2
};

/**
 * Centralized settings for OpenGL rendering quality.
 * Controls antialiasing, smoothing, and other visual quality options
 * that affect both camera (3D) and ortho (2D) views.
 */
class RenderingQualitySettings : public sigc::trackable
{
private:
    bool _lineAntialiasing;
    RenderHintLevel _lineSmoothHint;
    bool _pointAntialiasing;
    bool _multisampleEnabled;
    int _vertexPointSize;
    bool _vertexPointSmooth;

    sigc::signal<void> _sigSettingsChanged;

public:
    RenderingQualitySettings();

    // Accessors
    bool lineAntialiasingEnabled() const { return _lineAntialiasing; }
    RenderHintLevel lineSmoothHint() const { return _lineSmoothHint; }
    bool pointAntialiasingEnabled() const { return _pointAntialiasing; }
    bool multisampleEnabled() const { return _multisampleEnabled; }
    int vertexPointSize() const { return _vertexPointSize; }
    bool vertexPointSmooth() const { return _vertexPointSmooth; }

    // Signal emitted when any setting changes
    sigc::signal<void>& signal_settingsChanged() { return _sigSettingsChanged; }

    // Apply current line smoothing settings to OpenGL state
    void applyLineSmoothing() const;

    // Apply current point smoothing settings to OpenGL state
    void applyPointSmoothing() const;

    // Apply multisampling setting to OpenGL state
    void applyMultisampling() const;

    // Disable all smoothing (call after rendering smooth elements)
    void disableSmoothing() const;

    // Builds the preference page
    void constructPreferencePage();

private:
    void observeKey(const std::string& key);
    void keyChanged();
};

} // namespace ui

// Global accessor
ui::RenderingQualitySettings& GlobalRenderingQualitySettings();
