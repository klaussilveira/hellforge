#include "TerrainSculptTool.h"

#include "i18n.h"
#include "iundo.h"
#include "ipatch.h"
#include "itraceable.h"
#include "math/Ray.h"
#include "math/Matrix4.h"
#include "math/Vector4.h"

#include <wx/utils.h>

#include <algorithm>
#include <cmath>
#include <vector>

namespace ui
{

TerrainSculptSettings& TerrainSculptSettings::Instance()
{
    static TerrainSculptSettings instance;
    return instance;
}

const std::string& TerrainSculptTool::getName()
{
    static std::string name("TerrainSculptTool");
    return name;
}

const std::string& TerrainSculptTool::getDisplayName()
{
    static std::string displayName(_("Terrain Sculpt"));
    return displayName;
}

namespace
{
    bool buildRayFromEvent(MouseTool::Event& ev, Ray& outRay)
    {
        const VolumeTest& vol = ev.getInteractiveView().getVolumeTest();
        const Vector2& p = ev.getDevicePosition();

        Matrix4 invVP = vol.GetViewProjection().getFullInverse();

        Vector4 near4 = invVP.transform(Vector4(p.x(), p.y(), -1, 1));
        Vector4 far4  = invVP.transform(Vector4(p.x(), p.y(),  1, 1));

        if (near4.w() == 0 || far4.w() == 0) return false;

        Vector3 nearP(near4.x() / near4.w(), near4.y() / near4.w(), near4.z() / near4.w());
        Vector3 farP (far4.x()  / far4.w(),  far4.y()  / far4.w(),  far4.z()  / far4.w());

        outRay = Ray::createForPoints(nearP, farP);
        return true;
    }

    bool raycastTarget(const Ray& ray, const scene::INodePtr& node, IPatch*& patch, Vector3& point)
    {
        if (!node || !Node_isPatch(node)) return false;

        auto traceable = std::dynamic_pointer_cast<ITraceable>(node);
        if (!traceable) return false;

        if (!traceable->getIntersection(ray, point)) return false;

        patch = Node_getIPatch(node);
        return patch != nullptr;
    }
}


MouseTool::Result TerrainSculptTool::onMouseDown(Event& ev)
{
    auto& settings = TerrainSculptSettings::Instance();
    if (!settings.panelActive) return Result::Ignored;
    if (!settings.target.lock()) return Result::Ignored;

    if (GlobalUndoSystem().operationStarted()) return Result::Ignored;

    _stroking = true;
    _undoStarted = false;
    _strokeFlattenTargetSet = false;
    _strokeInverted = wxGetMouseState().RightIsDown();

    applyStrokeAt(ev);

    return Result::Activated;
}

MouseTool::Result TerrainSculptTool::onMouseMove(Event& ev)
{
    if (_stroking)
    {
        applyStrokeAt(ev);
        return Result::Continued;
    }

    auto& settings = TerrainSculptSettings::Instance();

    bool prevValid = settings.hoverValid;
    Vector3 prevPoint = settings.hoverPoint;

    settings.hoverValid = false;

    scene::INodePtr target = settings.panelActive ? settings.target.lock() : nullptr;

    if (target)
    {
        Ray ray;
        if (buildRayFromEvent(ev, ray))
        {
            IPatch* patch = nullptr;
            Vector3 point;
            if (raycastTarget(ray, target, patch, point))
            {
                settings.hoverValid = true;
                settings.hoverPoint = point;
            }
        }
    }

    if (settings.hoverValid != prevValid ||
        (settings.hoverValid && settings.hoverPoint != prevPoint))
    {
        ev.getInteractiveView().queueDraw();
    }

    return Result::Ignored;
}

MouseTool::Result TerrainSculptTool::onMouseUp(Event& ev)
{
    if (!_stroking) return Result::Ignored;

    _stroking = false;
    if (_undoStarted)
    {
        GlobalUndoSystem().finish("terrainSculpt");
        _undoStarted = false;
    }

    ev.getInteractiveView().queueDraw();
    return Result::Finished;
}

MouseTool::Result TerrainSculptTool::onCancel(IInteractiveView&)
{
    if (_stroking)
    {
        _stroking = false;
        if (_undoStarted)
        {
            GlobalUndoSystem().finish("terrainSculpt");
            _undoStarted = false;
        }
    }
    return Result::Finished;
}

void TerrainSculptTool::onMouseCaptureLost(IInteractiveView& view)
{
    onCancel(view);
}

bool TerrainSculptTool::applyStrokeAt(Event& ev)
{
    const auto& settings = TerrainSculptSettings::Instance();

    scene::INodePtr target = settings.target.lock();
    if (!target) return false;

    Ray ray;
    if (!buildRayFromEvent(ev, ray)) return false;

    IPatch* patch = nullptr;
    Vector3 center;
    if (!raycastTarget(ray, target, patch, center)) return false;

    TerrainSculptSettings::Instance().hoverValid = true;
    TerrainSculptSettings::Instance().hoverPoint = center;

    if (_stroking && !_undoStarted)
    {
        GlobalUndoSystem().start();
        _undoStarted = true;
    }

    patch->undoSave();

    bool touched = false;
    switch (settings.mode)
    {
    case TerrainSculptMode::Raise:
        touched = terrainSculpt::applyRaiseLower(patch, center, settings,
                                  _strokeInverted ? -1.0f : +1.0f);
        break;
    case TerrainSculptMode::Lower:
        touched = terrainSculpt::applyRaiseLower(patch, center, settings,
                                  _strokeInverted ? +1.0f : -1.0f);
        break;
    case TerrainSculptMode::Smooth:
        touched = terrainSculpt::applySmooth(patch, center, settings);
        break;
    case TerrainSculptMode::Flatten:
        if (!_strokeFlattenTargetSet)
        {
            _strokeFlattenTarget = settings.flattenHeightExplicit
                ? static_cast<double>(settings.flattenHeight)
                : center.z();
            _strokeFlattenTargetSet = true;
        }
        touched = terrainSculpt::applyFlatten(patch, center, _strokeFlattenTarget, settings);
        break;
    case TerrainSculptMode::Noise:
        touched = terrainSculpt::applyNoise(patch, center, settings);
        break;
    }

    if (touched)
    {
        patch->controlPointsChanged();
    }
    ev.getInteractiveView().queueDraw();

    return touched;
}

}
