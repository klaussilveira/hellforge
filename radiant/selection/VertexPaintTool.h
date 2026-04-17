#pragma once

#include "imousetool.h"
#include "inode.h"
#include "ivertexpaintable.h"
#include "math/Vector3.h"
#include <sigc++/signal.h>

namespace model
{
    class IIndexedModelSurface;
}

namespace ui
{

enum class VertexPaintChannel
{
    Red,
    Green,
    Blue
};

enum class VertexBrushFalloff
{
    Smooth,
    Linear,
    Spherical,
    Tip
};

struct VertexPaintSettings
{
    VertexPaintChannel channel = VertexPaintChannel::Red;
    model::PaintablePreviewMode previewMode = model::PaintablePreviewMode::Material;
    float radius = 32.0f;
    float strength = 0.5f;
    float falloff = 0.5f;
    VertexBrushFalloff falloffType = VertexBrushFalloff::Smooth;
    scene::INodeWeakPtr target;

    bool panelActive = false;
    bool hoverValid = false;
    Vector3 hoverPoint;

    sigc::signal<void> signal_settingsChanged;

    static VertexPaintSettings& Instance();
};

namespace vertexPaint
{
    double computeBrushWeight(double distNorm, float falloff, VertexBrushFalloff type);
    bool applyPaint(model::IIndexedModelSurface& surface, const Vector3& localCenter,
                    VertexPaintChannel channel, const VertexPaintSettings& s);
    bool fillAll(model::IIndexedModelSurface& surface, const Vector3& colour);
}

class VertexPaintTool : public MouseTool
{
private:
    bool _stroking = false;
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
