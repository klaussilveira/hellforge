#include "PlayerClearanceTool.h"

#include "i18n.h"
#include "iscenegraph.h"
#include "iselectiontest.h"
#include "itextstream.h"

#include "FaceIntersectionFinder.h"

namespace ui
{

namespace
{
    const Vector4 StandColour(0.2, 1.0, 0.2, 1.0);
    const Vector4 CrouchColour(1.0, 0.85, 0.1, 1.0);
    const Vector4 BlockedColour(1.0, 0.2, 0.2, 1.0);
}

using playerClearance::Fit;

PlayerClearanceTool::PlayerClearanceTool() :
    _hull(_hullBoxes, _colours),
    _eye(_eyeBoxes, _colours)
{}

const std::string& PlayerClearanceTool::getName()
{
    static std::string name("PlayerClearanceTool");
    return name;
}

const std::string& PlayerClearanceTool::getDisplayName()
{
    static std::string displayName(_("Player Clearance"));
    return displayName;
}

MouseTool::Result PlayerClearanceTool::onMouseDown(Event& ev)
{
    return Result::Ignored;
}

MouseTool::Result PlayerClearanceTool::onMouseMove(Event& ev)
{
    if (!_enabled || !_dims.isValid() || !GlobalSceneGraph().root())
    {
        return Result::Ignored;
    }

    Fit prevFit = _fit;
    Vector3 prevOrigin = _origin;

    _fit = Fit::None;

    SelectionTestPtr test = ev.getInteractiveView().createSelectionTestForPoint(ev.getDevicePosition());
    FaceIntersectionFinder finder(*test, test->getVolume().GetViewProjection());
    GlobalSceneGraph().root()->traverse(finder);

    FaceIntersection hit = finder.getResult();

    if (hit.valid && playerClearance::placeOnFloor(hit.point, hit.normal, _dims, _origin))
    {
        _fit = playerClearance::classify(GlobalSceneGraph().root(), _origin, _dims);
    }

    if (_fit != prevFit || (_fit != Fit::None && _origin != prevOrigin))
    {
        bool crouched = _fit == Fit::Crouch;
        double height = crouched ? _dims.crouchHeight : _dims.standHeight;
        double viewHeight = crouched ? _dims.crouchViewHeight : _dims.standViewHeight;
        double halfWidth = _dims.width * 0.5;

        _hullBoxes.assign(1, playerClearance::hullAt(_origin, _dims.width, height));
        _eyeBoxes.assign(1, AABB(_origin + Vector3(0, 0, viewHeight), Vector3(halfWidth, halfWidth, 0)));
        _colours.assign(1, _fit == Fit::Stand ? StandColour : (crouched ? CrouchColour : BlockedColour));

        _hull.queueUpdate();
        _eye.queueUpdate();

        ev.getInteractiveView().queueDraw();
    }

    return Result::Ignored;
}

MouseTool::Result PlayerClearanceTool::onMouseUp(Event& ev)
{
    return Result::Ignored;
}

void PlayerClearanceTool::onMouseLeave(IInteractiveView& view)
{
    if (_fit == Fit::None)
    {
        return;
    }

    _fit = Fit::None;
    view.queueDraw();
}

void PlayerClearanceTool::setEnabled(bool enabled)
{
    if (enabled == _enabled)
    {
        return;
    }

    _enabled = enabled;
    _fit = Fit::None;

    if (_enabled)
    {
        _dims = playerClearance::loadDimensions();

        if (!_dims.isValid())
        {
            rWarning() << "Player Clearance: the current game config has no <player> dimensions" << std::endl;
        }

        GlobalRenderSystem().attachRenderable(*this);
    }
    else
    {
        clearGeometry();
        _shader.reset();
        GlobalRenderSystem().detachRenderable(*this);
    }
}

void PlayerClearanceTool::clearGeometry()
{
    _hullBoxes.clear();
    _eyeBoxes.clear();
    _hull.clear();
    _eye.clear();
}

void PlayerClearanceTool::onPreRender(const VolumeTest& volume)
{
    if (_fit == Fit::None)
    {
        clearGeometry();
        return;
    }

    if (!volume.fill())
    {
        _hull.hide();
        _eye.hide();
        return;
    }

    if (!_shader)
    {
        _shader = GlobalRenderSystem().capture(ColourShaderType::CameraAndOrthoViewOutline, StandColour);
    }

    _hull.update(_shader);
    _eye.update(_shader);
}

}
