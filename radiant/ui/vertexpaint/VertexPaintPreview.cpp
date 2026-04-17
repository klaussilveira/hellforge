#include "VertexPaintPreview.h"

#include "imap.h"
#include "selection/VertexPaintTool.h"

#include <algorithm>
#include <cmath>

namespace ui
{

namespace
{
    constexpr int CircleSegments = 48;
    constexpr float ZOffset = 0.5f;
    constexpr double TwoPi = 6.283185307179586;
    const Vector4 BrushColour(0.2, 0.85, 1.0, 1.0);
    const Vector4 InnerColour(0.6, 1.0, 1.0, 1.0);
}

VertexPaintPreview::VertexPaintPreview() :
    _circle(_circleVertices),
    _innerCircle(_innerCircleVertices)
{
    _circle.setColour(BrushColour);
    _innerCircle.setColour(InnerColour);
}

void VertexPaintPreview::clear()
{
    _circleVertices.clear();
    _innerCircleVertices.clear();
    _circle.clear();
    _innerCircle.clear();
}

void VertexPaintPreview::onPreRender(const VolumeTest& volume)
{
    if (!volume.fill()) return;

    const auto& settings = VertexPaintSettings::Instance();

    _circleVertices.clear();
    _innerCircleVertices.clear();

    if (!settings.hoverValid)
    {
        _circle.clear();
        _innerCircle.clear();
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

    _circle.queueUpdate();
    _innerCircle.queueUpdate();
    _circle.update(_lineShader);
    _innerCircle.update(_innerLineShader);
}

}
