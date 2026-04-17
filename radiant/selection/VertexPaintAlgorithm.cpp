#include "VertexPaintTool.h"

#include "imodelsurface.h"

#include <algorithm>
#include <cmath>

namespace ui
{
namespace vertexPaint
{

double computeBrushWeight(double distNorm, float falloff, VertexBrushFalloff type)
{
    if (distNorm >= 1.0) return 0.0;
    if (falloff <= 0.0f) return 1.0;

    double begin = 1.0 - falloff;
    if (distNorm <= begin) return 1.0;

    double t = (distNorm - begin) / falloff;
    if (t < 0) t = 0;
    if (t > 1) t = 1;

    switch (type)
    {
    case VertexBrushFalloff::Linear:
        return 1.0 - t;
    case VertexBrushFalloff::Smooth:
        return 1.0 - (t * t * (3.0 - 2.0 * t));
    case VertexBrushFalloff::Spherical:
        return std::sqrt(std::max(0.0, 1.0 - t * t));
    case VertexBrushFalloff::Tip:
    {
        double tp = 1.0 - t;
        return 1.0 - std::sqrt(std::max(0.0, 1.0 - tp * tp));
    }
    }
    return 1.0 - t;
}

bool applyPaint(model::IIndexedModelSurface& surface, const Vector3& localCenter,
                VertexPaintChannel channel, const VertexPaintSettings& s)
{
    auto* verts = surface.getMutableVertexArray();
    if (!verts) return false;

    const float radius = s.radius;
    const float radiusSq = radius * radius;
    const double strength = std::clamp(static_cast<double>(s.strength), 0.0, 1.0);
    bool touched = false;

    auto lerp01 = [](double current, double target, double blend) {
        double next = current + (target - current) * blend;
        if (next < 0.0) next = 0.0;
        if (next > 1.0) next = 1.0;
        return next;
    };

    double targetR = channel == VertexPaintChannel::Red   ? 1.0 : 0.0;
    double targetG = channel == VertexPaintChannel::Green ? 1.0 : 0.0;
    double targetB = channel == VertexPaintChannel::Blue  ? 1.0 : 0.0;

    for (auto& v : *verts)
    {
        double dx = v.vertex.x() - localCenter.x();
        double dy = v.vertex.y() - localCenter.y();
        double dz = v.vertex.z() - localCenter.z();
        double distSq = dx * dx + dy * dy + dz * dz;
        if (distSq > radiusSq) continue;

        double weight = computeBrushWeight(std::sqrt(distSq) / radius, s.falloff, s.falloffType);
        if (weight <= 0) continue;

        double blend = std::clamp(strength * weight, 0.0, 1.0);
        v.colour.x() = lerp01(v.colour.x(), targetR, blend);
        v.colour.y() = lerp01(v.colour.y(), targetG, blend);
        v.colour.z() = lerp01(v.colour.z(), targetB, blend);

        touched = true;
    }
    return touched;
}

bool fillAll(model::IIndexedModelSurface& surface, const Vector3& colour)
{
    auto* verts = surface.getMutableVertexArray();
    if (!verts || verts->empty()) return false;

    for (auto& v : *verts)
    {
        v.colour.x() = colour.x();
        v.colour.y() = colour.y();
        v.colour.z() = colour.z();
    }
    return true;
}

} // namespace vertexPaint
} // namespace ui
