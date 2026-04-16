#include "TerrainSculptTool.h"

#include "ipatch.h"
#include "noise/Noise.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace ui
{
namespace terrainSculpt
{

double computeBrushWeight(double distNorm, float falloff, TerrainBrushFalloff type)
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
    case TerrainBrushFalloff::Linear:
        return 1.0 - t;
    case TerrainBrushFalloff::Smooth:
        return 1.0 - (t * t * (3.0 - 2.0 * t));
    case TerrainBrushFalloff::Spherical:
        return std::sqrt(std::max(0.0, 1.0 - t * t));
    case TerrainBrushFalloff::Tip:
    {
        double tp = 1.0 - t;
        return 1.0 - std::sqrt(std::max(0.0, 1.0 - tp * tp));
    }
    }
    return 1.0 - t;
}

bool applyRaiseLower(IPatch* patch, const Vector3& center, const TerrainSculptSettings& s, float direction)
{
    const float radius = s.radius;
    const float radiusSq = radius * radius;
    const float amount = s.strength * direction;
    const std::size_t w = patch->getWidth();
    const std::size_t h = patch->getHeight();
    bool touched = false;

    for (std::size_t row = 0; row < h; ++row)
    for (std::size_t col = 0; col < w; ++col)
    {
        PatchControl& ctrl = patch->ctrlAt(row, col);
        double dx = ctrl.vertex.x() - center.x();
        double dy = ctrl.vertex.y() - center.y();
        double distSq = dx * dx + dy * dy;
        if (distSq > radiusSq) continue;

        double weight = computeBrushWeight(std::sqrt(distSq) / radius, s.falloff, s.falloffType);
        if (weight <= 0) continue;

        ctrl.vertex.z() += amount * weight;
        touched = true;
    }
    return touched;
}

bool applySmooth(IPatch* patch, const Vector3& center, const TerrainSculptSettings& s)
{
    const std::size_t w = patch->getWidth();
    const std::size_t h = patch->getHeight();
    const float radius = s.radius;
    const float radiusSq = radius * radius;
    const float kernel = std::max(0.01f, s.smoothFilterRadius * radius);
    const float kernelSq = kernel * kernel;
    const double blendBase = std::clamp(s.strength / 100.0, 0.0, 1.0);

    std::vector<double> originalZ(w * h);
    for (std::size_t row = 0; row < h; ++row)
    for (std::size_t col = 0; col < w; ++col)
    {
        originalZ[row * w + col] = patch->ctrlAt(row, col).vertex.z();
    }

    bool touched = false;

    for (std::size_t row = 0; row < h; ++row)
    for (std::size_t col = 0; col < w; ++col)
    {
        PatchControl& ctrl = patch->ctrlAt(row, col);
        double dx = ctrl.vertex.x() - center.x();
        double dy = ctrl.vertex.y() - center.y();
        double distSq = dx * dx + dy * dy;
        if (distSq > radiusSq) continue;

        double brushWeight = computeBrushWeight(std::sqrt(distSq) / radius, s.falloff, s.falloffType);
        if (brushWeight <= 0) continue;

        double sum = 0.0;
        int count = 0;
        for (std::size_t r2 = 0; r2 < h; ++r2)
        for (std::size_t c2 = 0; c2 < w; ++c2)
        {
            const PatchControl& other = patch->ctrlAt(r2, c2);
            double ox = other.vertex.x() - ctrl.vertex.x();
            double oy = other.vertex.y() - ctrl.vertex.y();
            if (ox * ox + oy * oy > kernelSq) continue;
            sum += originalZ[r2 * w + c2];
            ++count;
        }
        if (count == 0) continue;

        double avg = sum / count;
        double currentZ = originalZ[row * w + col];
        double blend = std::clamp(blendBase * brushWeight, 0.0, 1.0);
        ctrl.vertex.z() = currentZ + (avg - currentZ) * blend;
        touched = true;
    }
    return touched;
}

bool applyNoise(IPatch* patch, const Vector3& center, const TerrainSculptSettings& s)
{
    const std::size_t w = patch->getWidth();
    const std::size_t h = patch->getHeight();
    const float radius = s.radius;
    const float radiusSq = radius * radius;
    const double blendBase = std::clamp(s.strength / 100.0, 0.0, 1.0);
    const double amount = s.noiseAmount;

    noise::NoiseParameters params;
    params.algorithm = s.noiseAlgorithm;
    params.seed = s.noiseSeed;
    params.frequency = s.noiseScale;
    params.amplitude = 1.0;
    noise::NoiseGenerator gen(params);

    bool touched = false;

    for (std::size_t row = 0; row < h; ++row)
    for (std::size_t col = 0; col < w; ++col)
    {
        PatchControl& ctrl = patch->ctrlAt(row, col);
        double dx = ctrl.vertex.x() - center.x();
        double dy = ctrl.vertex.y() - center.y();
        double distSq = dx * dx + dy * dy;
        if (distSq > radiusSq) continue;

        double brushWeight = computeBrushWeight(std::sqrt(distSq) / radius, s.falloff, s.falloffType);
        if (brushWeight <= 0) continue;

        double noiseVal = gen.sample(ctrl.vertex.x(), ctrl.vertex.y());
        ctrl.vertex.z() += noiseVal * amount * brushWeight * blendBase;
        touched = true;
    }
    return touched;
}

bool applyFlatten(IPatch* patch, const Vector3& center, double target, const TerrainSculptSettings& s)
{
    const std::size_t w = patch->getWidth();
    const std::size_t h = patch->getHeight();
    const float radius = s.radius;
    const float radiusSq = radius * radius;
    const double blendBase = std::clamp(s.strength / 100.0, 0.0, 1.0);
    bool touched = false;

    for (std::size_t row = 0; row < h; ++row)
    for (std::size_t col = 0; col < w; ++col)
    {
        PatchControl& ctrl = patch->ctrlAt(row, col);
        double dx = ctrl.vertex.x() - center.x();
        double dy = ctrl.vertex.y() - center.y();
        double distSq = dx * dx + dy * dy;
        if (distSq > radiusSq) continue;

        double brushWeight = computeBrushWeight(std::sqrt(distSq) / radius, s.falloff, s.falloffType);
        if (brushWeight <= 0) continue;

        double blend = std::clamp(blendBase * brushWeight, 0.0, 1.0);
        double currentZ = ctrl.vertex.z();
        ctrl.vertex.z() = currentZ + (target - currentZ) * blend;
        touched = true;
    }
    return touched;
}

} // namespace terrainSculpt
} // namespace ui
