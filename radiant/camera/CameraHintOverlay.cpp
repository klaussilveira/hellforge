#include "CameraHintOverlay.h"

#include "igl.h"
#include "iclipper.h"
#include "iorthoview.h"
#include "iselection.h"

#include <vector>
#include <string>

namespace ui
{

namespace
{

constexpr float TEXT_R = 1.0f, TEXT_G = 1.0f, TEXT_B = 1.0f, TEXT_A = 0.85f;
constexpr float BG_R = 0.0f, BG_G = 0.0f, BG_B = 0.0f, BG_A = 0.5f;
constexpr float PADDING_X = 8.0f;
constexpr float PADDING_Y = 8.0f;
constexpr float LINE_HEIGHT = 16.0f;
constexpr float TEXT_PADDING = 4.0f; // extra padding inside background rect

std::vector<std::string> getHintLines()
{
    // Context hints based on current mode/selection, with all relevant
    // modifier combinations shown upfront so users can discover them.
    if (GlobalOrthoViewManager().polygonMode())
    {
        return {
            "LMB: Place vertex | Return: Finish | ESC: Cancel"
        };
    }

    if (GlobalClipper().clipMode())
    {
        return {
            "LMB: Place clip point | Return: Clip | ESC: Cancel"
        };
    }

    if (GlobalSelectionSystem().countSelected() > 0)
    {
        return {
            "Drag: Move | CTRL+Drag: Disable grid snap",
            "Drag+SHIFT: Lock XY | Drag+ALT: Lock Z",
            "ALT+Scroll: Resize face | CTRL+Scroll: Symmetric resize"
        };
    }

    // Default: nothing selected
    return {
        "LMB: Select | SHIFT+LMB: Add to selection",
        "Drag+SHIFT: Lock XY | Drag+ALT: Lock Z"
    };
}

} // namespace

void CameraHintOverlay::render(int width, int height, const IGLFont::Ptr& font)
{
    auto lines = getHintLines();
    if (lines.empty())
        return;

    auto numLines = static_cast<float>(lines.size());
    float blockHeight = numLines * LINE_HEIGHT + TEXT_PADDING * 2.0f;
    float blockTop = 10.0f;

    // Estimate block width from longest line (approximate 7px per char)
    float maxWidth = 0;
    for (const auto& line : lines)
    {
        float w = static_cast<float>(line.size()) * 7.0f;
        if (w > maxWidth) maxWidth = w;
    }
    float blockWidth = maxWidth + TEXT_PADDING * 2.0f;

    // Draw semi-transparent background rectangle
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glColor4f(BG_R, BG_G, BG_B, BG_A);
    glBegin(GL_QUADS);
    glVertex2f(PADDING_X, blockTop);
    glVertex2f(PADDING_X + blockWidth, blockTop);
    glVertex2f(PADDING_X + blockWidth, blockTop + blockHeight);
    glVertex2f(PADDING_X, blockTop + blockHeight);
    glEnd();

    // Draw text lines top-to-bottom within the block
    glColor4f(TEXT_R, TEXT_G, TEXT_B, TEXT_A);
    float textX = PADDING_X + TEXT_PADDING;
    float textY = blockTop + TEXT_PADDING + LINE_HEIGHT;

    for (const auto& line : lines)
    {
        glRasterPos3f(textX, textY, 0.0f);
        font->drawString(line);
        textY += LINE_HEIGHT;
    }

    glDisable(GL_BLEND);
}

} // namespace ui
