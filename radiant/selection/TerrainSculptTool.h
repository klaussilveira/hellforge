#pragma once

#include "imousetool.h"
#include "inode.h"
#include "ipatch.h"
#include "math/Vector3.h"
#include "noise/Noise.h"
#include <sigc++/signal.h>

namespace ui
{

enum class TerrainSculptMode
{
    Raise,
    Lower,
    Smooth,
    Flatten,
    Noise
};

enum class TerrainBrushFalloff
{
    Smooth,
    Linear,
    Spherical,
    Tip
};

struct TerrainSculptSettings
{
    TerrainSculptMode mode = TerrainSculptMode::Raise;
    float radius = 64.0f;
    float strength = 8.0f;
    float falloff = 0.5f;
    TerrainBrushFalloff falloffType = TerrainBrushFalloff::Smooth;
    float smoothFilterRadius = 1.0f;
    float flattenHeight = 0.0f;
    bool flattenHeightExplicit = false;
    noise::Algorithm noiseAlgorithm = noise::Algorithm::Perlin;
    float noiseScale = 0.05f;
    float noiseAmount = 16.0f;
    unsigned int noiseSeed = 0;
    scene::INodeWeakPtr target;

    bool panelActive = false;
    bool hoverValid = false;
    Vector3 hoverPoint;

    sigc::signal<void> signal_settingsChanged;

    static TerrainSculptSettings& Instance();
};

namespace terrainSculpt
{
    double computeBrushWeight(double distNorm, float falloff, TerrainBrushFalloff type);
    bool applyRaiseLower(IPatch* patch, const Vector3& center, const TerrainSculptSettings& s, float direction);
    bool applySmooth(IPatch* patch, const Vector3& center, const TerrainSculptSettings& s);
    bool applyFlatten(IPatch* patch, const Vector3& center, double target, const TerrainSculptSettings& s);
    bool applyNoise(IPatch* patch, const Vector3& center, const TerrainSculptSettings& s);
}

class TerrainSculptTool : public MouseTool
{
private:
    bool _stroking = false;
    bool _undoStarted = false;
    bool _strokeInverted = false;
    bool _strokeFlattenTargetSet = false;
    double _strokeFlattenTarget = 0.0;
    scene::INodeWeakPtr _strokeTarget;

public:
    const std::string& getName() override;
    const std::string& getDisplayName() override;

    Result onMouseDown(Event& ev) override;
    Result onMouseMove(Event& ev) override;
    Result onMouseUp(Event& ev) override;

    Result onCancel(IInteractiveView& view) override;
    void onMouseCaptureLost(IInteractiveView& view) override;

    unsigned int getPointerMode() override { return PointerMode::Capture; }
    bool allowChaseMouse() override { return false; }
    bool alwaysReceivesMoveEvents() override { return true; }

private:
    bool applyStrokeAt(Event& ev);
};

}
