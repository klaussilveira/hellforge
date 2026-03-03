#pragma once

#include "igl.h"

namespace ui
{

/// Renders context-sensitive keyboard shortcut hints in the 3D viewport corner
class CameraHintOverlay
{
public:
    /// Render hints in the bottom-left corner during the 2D overlay phase of Cam_Draw.
    /// width/height = viewport dimensions, font = current GL font.
    static void render(int width, int height, const IGLFont::Ptr& font);
};

} // namespace ui
