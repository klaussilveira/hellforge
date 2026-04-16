#include "TerrainSculptPreview.h"

#include "imap.h"
#include "ipatch.h"
#include "selection/TerrainSculptTool.h"

#include <algorithm>
#include <cmath>

namespace ui
{

namespace
{
    constexpr int CircleSegments = 48;
    constexpr float ZOffset = 0.5f;
    constexpr double TwoPi = 6.283185307179586;
    const Vector4 BrushColour(1.0, 0.85, 0.2, 1.0);
    const Vector4 InnerColour(1.0, 1.0, 0.6, 1.0);
    const Vector4 PointColour(1.0, 0.5, 0.1, 1.0);
}

TerrainSculptPreview::TerrainSculptPreview() :
    _circle(_circleVertices),
    _innerCircle(_innerCircleVertices),
    _points(_pointVertices)
{
    _circle.setColour(BrushColour);
    _innerCircle.setColour(InnerColour);
    _points.setColour(PointColour);
}

void TerrainSculptPreview::clear()
{
    _circleVertices.clear();
    _innerCircleVertices.clear();
    _pointVertices.clear();
    _circle.clear();
    _innerCircle.clear();
    _points.clear();
}

void TerrainSculptPreview::onPreRender(const VolumeTest& volume)
{
    if (!volume.fill()) return; // camera views only

    const auto& settings = TerrainSculptSettings::Instance();

    _circleVertices.clear();
    _innerCircleVertices.clear();
    _pointVertices.clear();

    if (!settings.hoverValid)
    {
        _circle.clear();
        _innerCircle.clear();
        _points.clear();
        return;
    }

    auto renderSystem = GlobalMapModule().getRoot()
        ? GlobalMapModule().getRoot()->getRenderSystem()
        : RenderSystemPtr();
    if (!renderSystem) return;

    if (!_lineShader)
    {
        _lineShader = renderSystem->capture(ColourShaderType::CameraOutline, BrushColour);
    }
    if (!_innerLineShader)
    {
        _innerLineShader = renderSystem->capture(ColourShaderType::CameraOutline, InnerColour);
    }
    if (!_pointShader)
    {
        _pointShader = renderSystem->capture(BuiltInShaderType::BigPoint);
    }

    const Vector3& c = settings.hoverPoint;
    const double r = settings.radius;
    const double innerR = r * std::max(0.0, 1.0 - static_cast<double>(settings.falloff));
    const double z = c.z() + ZOffset;

    for (int i = 0; i <= CircleSegments; ++i)
    {
        double a = (TwoPi * i) / CircleSegments;
        double cx = std::cos(a);
        double cy = std::sin(a);
        _circleVertices.emplace_back(c.x() + r * cx, c.y() + r * cy, z);
        if (innerR > 0.01 && innerR < r - 0.01)
        {
            _innerCircleVertices.emplace_back(c.x() + innerR * cx, c.y() + innerR * cy, z);
        }
    }

    if (auto target = settings.target.lock())
    {
        if (IPatch* patch = Node_getIPatch(target))
        {
            const std::size_t w = patch->getWidth();
            const std::size_t h = patch->getHeight();
            const double rSq = r * r;

            for (std::size_t row = 0; row < h; ++row)
            for (std::size_t col = 0; col < w; ++col)
            {
                const PatchControl& ctrl = patch->ctrlAt(row, col);
                double dx = ctrl.vertex.x() - c.x();
                double dy = ctrl.vertex.y() - c.y();
                if (dx * dx + dy * dy > rSq) continue;

                _pointVertices.emplace_back(ctrl.vertex.x(),
                                            ctrl.vertex.y(),
                                            ctrl.vertex.z() + ZOffset);
            }
        }
    }

    _circle.queueUpdate();
    _innerCircle.queueUpdate();
    _points.queueUpdate();
    _circle.update(_lineShader);
    _innerCircle.update(_innerLineShader);
    _points.update(_pointShader);
}

}
