#include "RenderingQualitySettings.h"

#include "i18n.h"
#include "igl.h"
#include "ipreferencesystem.h"
#include "registry/registry.h"

namespace ui
{

RenderingQualitySettings::RenderingQualitySettings() :
    _lineAntialiasing(registry::getValue<bool>(RKEY_LINE_ANTIALIASING, true)),
    _lineSmoothHint(static_cast<RenderHintLevel>(registry::getValue<int>(RKEY_LINE_SMOOTH_HINT, 2))),
    _pointAntialiasing(registry::getValue<bool>(RKEY_POINT_ANTIALIASING, true)),
    _multisampleEnabled(registry::getValue<bool>(RKEY_MULTISAMPLE_ENABLED, true)),
    _vertexPointSize(registry::getValue<int>(RKEY_VERTEX_POINT_SIZE, 8)),
    _vertexPointSmooth(registry::getValue<bool>(RKEY_VERTEX_POINT_SMOOTH, true))
{
    // Observe registry keys for changes
    observeKey(RKEY_LINE_ANTIALIASING);
    observeKey(RKEY_LINE_SMOOTH_HINT);
    observeKey(RKEY_POINT_ANTIALIASING);
    observeKey(RKEY_MULTISAMPLE_ENABLED);
    observeKey(RKEY_VERTEX_POINT_SIZE);
    observeKey(RKEY_VERTEX_POINT_SMOOTH);

    // Build preferences page
    constructPreferencePage();
}

void RenderingQualitySettings::observeKey(const std::string& key)
{
    GlobalRegistry().signalForKey(key).connect(
        sigc::mem_fun(this, &RenderingQualitySettings::keyChanged)
    );
}

void RenderingQualitySettings::keyChanged()
{
    _lineAntialiasing = registry::getValue<bool>(RKEY_LINE_ANTIALIASING, true);
    _lineSmoothHint = static_cast<RenderHintLevel>(registry::getValue<int>(RKEY_LINE_SMOOTH_HINT, 2));
    _pointAntialiasing = registry::getValue<bool>(RKEY_POINT_ANTIALIASING, true);
    _multisampleEnabled = registry::getValue<bool>(RKEY_MULTISAMPLE_ENABLED, true);
    _vertexPointSize = registry::getValue<int>(RKEY_VERTEX_POINT_SIZE, 8);
    _vertexPointSmooth = registry::getValue<bool>(RKEY_VERTEX_POINT_SMOOTH, true);

    // Clamp point size
    if (_vertexPointSize < 4) _vertexPointSize = 4;
    if (_vertexPointSize > 16) _vertexPointSize = 16;

    _sigSettingsChanged.emit();
}

void RenderingQualitySettings::applyLineSmoothing() const
{
    if (_multisampleEnabled)
    {
        glEnable(GL_MULTISAMPLE);
    }

    if (_lineAntialiasing)
    {
        glEnable(GL_LINE_SMOOTH);

        GLenum hint = GL_DONT_CARE;
        switch (_lineSmoothHint)
        {
            case RenderHintLevel::Fastest:  hint = GL_FASTEST; break;
            case RenderHintLevel::DontCare: hint = GL_DONT_CARE; break;
            case RenderHintLevel::Nicest:   hint = GL_NICEST; break;
        }
        glHint(GL_LINE_SMOOTH_HINT, hint);
    }
}

void RenderingQualitySettings::applyPointSmoothing() const
{
    if (_pointAntialiasing || _vertexPointSmooth)
    {
        glEnable(GL_POINT_SMOOTH);
        glHint(GL_POINT_SMOOTH_HINT, GL_NICEST);
    }
}

void RenderingQualitySettings::applyMultisampling() const
{
    if (_multisampleEnabled)
    {
        glEnable(GL_MULTISAMPLE);
    }
}

void RenderingQualitySettings::disableSmoothing() const
{
    glDisable(GL_LINE_SMOOTH);
    glDisable(GL_POINT_SMOOTH);
    glDisable(GL_MULTISAMPLE);
}

void RenderingQualitySettings::constructPreferencePage()
{
    IPreferencePage& page = GlobalPreferenceSystem().getPage(_("Rendering Quality"));

    page.appendCheckBox(_("Enable multisampling (MSAA)"), RKEY_MULTISAMPLE_ENABLED);
    page.appendCheckBox(_("Enable line antialiasing"), RKEY_LINE_ANTIALIASING);

    ComboBoxValueList hintLevels;
    hintLevels.push_back(_("Fastest"));
    hintLevels.push_back(_("Default"));
    hintLevels.push_back(_("Nicest"));
    page.appendCombo(_("Line smoothing quality"), RKEY_LINE_SMOOTH_HINT, hintLevels, true);

    page.appendCheckBox(_("Enable point antialiasing"), RKEY_POINT_ANTIALIASING);

    page.appendCheckBox(_("Smooth vertex points (round instead of square)"), RKEY_VERTEX_POINT_SMOOTH);

    ComboBoxValueList pointSizes;
    pointSizes.push_back("4");
    pointSizes.push_back("6");
    pointSizes.push_back("8");
    pointSizes.push_back("10");
    pointSizes.push_back("12");
    pointSizes.push_back("16");
    page.appendCombo(_("Vertex point size"), RKEY_VERTEX_POINT_SIZE, pointSizes, true);
}

} // namespace ui

ui::RenderingQualitySettings& GlobalRenderingQualitySettings()
{
    static ui::RenderingQualitySettings _settings;
    return _settings;
}
