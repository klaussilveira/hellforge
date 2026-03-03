#include "CameraBrushCreatorTool.h"

#include "i18n.h"
#include "igrid.h"
#include "iclipper.h"
#include "icommandsystem.h"
#include "ibrush.h"
#include "imousetoolmanager.h"
#include "iscenegraph.h"
#include "scenelib.h"
#include "selectionlib.h"
#include "command/ExecutionNotPossible.h"
#include "CameraMouseToolEvent.h"
#include "FaceIntersectionFinder.h"
#include "iorthoview.h"
#include "ui/texturebrowser/TextureBrowserPanel.h"
#include "ui/texturebrowser/TextureBrowserManager.h"
#include "selection/SelectionVolume.h"
#include "selection/Device.h"
#include "Rectangle.h"
#include "registry/registry.h"
#include "math/Plane3.h"
#include "camera/CameraWndManager.h"
#include "wxutil/Modifier.h"
#include "igl.h"

#include <wx/utils.h>

namespace ui
{

namespace
{
    // Check if all modifiers in the given flags are currently held
    bool areModifiersHeld(unsigned int flags)
    {
        if (flags == wxutil::Modifier::NONE) return false;

        if ((flags & wxutil::Modifier::SHIFT) && !wxGetKeyState(WXK_SHIFT)) return false;
        if ((flags & wxutil::Modifier::ALT) && !wxGetKeyState(WXK_ALT)) return false;
        if ((flags & wxutil::Modifier::CONTROL) && !wxGetKeyState(WXK_CONTROL)) return false;

        return true;
    }
}

const std::string& CameraBrushCreatorTool::getName()
{
    static std::string name("CameraBrushCreatorTool");
    return name;
}

const std::string& CameraBrushCreatorTool::getDisplayName()
{
    static std::string displayName(_("Drag-create Brush (3D)"));
    return displayName;
}

Ray CameraBrushCreatorTool::calculateRayForDevicePoint(camera::ICameraView& camView, const Vector2& devicePoint) const
{
    // Get the camera's position and orientation
    const Vector3& camOrigin = camView.getCameraOrigin();
    const Matrix4& modelView = camView.getModelView();
    const Matrix4& projection = camView.getProjection();

    // Combine projection and modelview matrices
    Matrix4 viewProj = projection.getMultipliedBy(modelView);
    Matrix4 invViewProj = viewProj.getFullInverse();

    // Convert device coordinates to clip space (device coords are already normalized -1 to 1)
    Vector3 nearPoint(devicePoint.x(), devicePoint.y(), -1.0);
    Vector3 farPoint(devicePoint.x(), devicePoint.y(), 1.0);

    // Transform to world space
    Vector4 nearWorld4 = invViewProj.transform(Vector4(nearPoint.x(), nearPoint.y(), nearPoint.z(), 1.0));
    Vector4 farWorld4 = invViewProj.transform(Vector4(farPoint.x(), farPoint.y(), farPoint.z(), 1.0));

    // Perspective divide
    Vector3 nearWorld = nearWorld4.getProjected();
    Vector3 farWorld = farWorld4.getProjected();

    // Create ray from near to far
    return Ray::createForPoints(nearWorld, farWorld);
}

Vector3 CameraBrushCreatorTool::getWorldPosOnPlane(const Ray& ray, double planeZ) const
{
    // Create a horizontal plane at the given Z
    Plane3 plane(0, 0, 1, -planeZ);

    // Calculate the distance along the ray to the plane intersection
    double distance = ray.getDistance(plane);

    // Return the intersection point
    return ray.origin + ray.direction * distance;
}

void CameraBrushCreatorTool::snapToGrid(Vector3& pos) const
{
    double gridSize = GlobalGrid().getGridSize();
    pos.x() = float_snapped(pos.x(), gridSize);
    pos.y() = float_snapped(pos.y(), gridSize);
    pos.z() = float_snapped(pos.z(), gridSize);
}

void CameraBrushCreatorTool::applyConstraints(Vector3& mins, Vector3& maxs, bool shiftHeld, bool altHeld) const
{
    double gridSize = GlobalGrid().getGridSize();

    // mins is always anchored at _startPos - we only modify maxs for constraints
    if (shiftHeld && altHeld)
    {
        // Shift+Alt: X = Y = Z (cube)
        double sizeX = maxs.x() - mins.x();
        double sizeY = maxs.y() - mins.y();
        double sizeZ = maxs.z() - mins.z();

        // Use the largest dimension
        double maxSize = std::max({std::abs(sizeX), std::abs(sizeY), std::abs(sizeZ)});
        if (maxSize < gridSize) maxSize = gridSize;

        // Adjust maxs to create a cube, preserving the direction of growth
        // Keep mins fixed, only move maxs
        maxs.x() = mins.x() + (sizeX >= 0 ? maxSize : -maxSize);
        maxs.y() = mins.y() + (sizeY >= 0 ? maxSize : -maxSize);
        maxs.z() = mins.z() + maxSize;
    }
    else if (shiftHeld)
    {
        // Shift only: X = Y (square base)
        double sizeX = maxs.x() - mins.x();
        double sizeY = maxs.y() - mins.y();

        // Use the larger dimension
        double maxXY = std::max(std::abs(sizeX), std::abs(sizeY));
        if (maxXY < gridSize) maxXY = gridSize;

        // Adjust maxs to create square base, preserving direction
        // Keep mins fixed, only move maxs
        maxs.x() = mins.x() + (sizeX >= 0 ? maxXY : -maxXY);
        maxs.y() = mins.y() + (sizeY >= 0 ? maxXY : -maxXY);
    }
    // Alt-only is handled in onMouseMove before this function is called
}

bool CameraBrushCreatorTool::findSurfaceUnderCursor(CameraMouseToolEvent& camEvent,
    const Vector2& devicePos, Vector3& outPoint, Vector3& outNormal)
{
    SelectionTestPtr selectionTest = camEvent.getView().createSelectionTestForPoint(devicePos);
    const Matrix4& viewProjection = selectionTest->getVolume().GetViewProjection();

    FaceIntersectionFinder finder(*selectionTest, viewProjection);
    GlobalSceneGraph().root()->traverse(finder);

    const FaceIntersection& intersection = finder.getResult();

    if (intersection.valid)
    {
        outPoint = intersection.point;
        outNormal = intersection.normal;
        return true;
    }

    return false;
}

void CameraBrushCreatorTool::cacheNearbyBrushes(const Vector3& center, double radius)
{
    _nearbyBrushes.clear();

    AABB searchAABB(center, Vector3(radius, radius, radius));

    GlobalSceneGraph().foreachVisibleNode([&](const scene::INodePtr& node) -> bool
    {
        if (!Node_isBrush(node))
            return true; // continue

        // Skip our own WIP brush
        if (node == _brush)
            return true;

        AABB brushAABB = node->worldAABB();

        if (searchAABB.intersects(brushAABB))
        {
            _nearbyBrushes.push_back({brushAABB});
        }

        return true; // continue
    });
}

double CameraBrushCreatorTool::snapToNearbyFace(double value, int axis, double snapThreshold) const
{
    double bestSnap = value;
    double bestDist = snapThreshold;

    for (const auto& nb : _nearbyBrushes)
    {
        double faceMin = nb.aabb.origin[axis] - nb.aabb.extents[axis];
        double faceMax = nb.aabb.origin[axis] + nb.aabb.extents[axis];

        double distMin = std::abs(value - faceMin);
        double distMax = std::abs(value - faceMax);

        if (distMin < bestDist)
        {
            bestSnap = faceMin;
            bestDist = distMin;
        }
        if (distMax < bestDist)
        {
            bestSnap = faceMax;
            bestDist = distMax;
        }
    }

    return bestSnap;
}

void CameraBrushCreatorTool::clampToPreventOverlap(const Vector3& anchor,
    Vector3& mins, Vector3& maxs) const
{
    AABB proposedAABB = AABB::createFromMinMax(mins, maxs);

    for (const auto& nb : _nearbyBrushes)
    {
        if (!proposedAABB.intersects(nb.aabb))
            continue;

        Vector3 nbMin(
            nb.aabb.origin.x() - nb.aabb.extents.x(),
            nb.aabb.origin.y() - nb.aabb.extents.y(),
            nb.aabb.origin.z() - nb.aabb.extents.z()
        );
        Vector3 nbMax(
            nb.aabb.origin.x() + nb.aabb.extents.x(),
            nb.aabb.origin.y() + nb.aabb.extents.y(),
            nb.aabb.origin.z() + nb.aabb.extents.z()
        );

        for (int axis = 0; axis < 3; axis++)
        {
            // Skip this axis if the anchor sits on a face of this brush —
            // that means we started construction on this surface and should
            // be allowed to grow away from it.
            double eps = 0.1;
            if (std::abs(anchor[axis] - nbMin[axis]) < eps ||
                std::abs(anchor[axis] - nbMax[axis]) < eps)
                continue;

            // Only clamp the edge that's being dragged (away from anchor)
            if (anchor[axis] <= nbMin[axis] && maxs[axis] > nbMin[axis])
            {
                maxs[axis] = nbMin[axis]; // stop at brush's min face
            }
            if (anchor[axis] >= nbMax[axis] && mins[axis] < nbMax[axis])
            {
                mins[axis] = nbMax[axis]; // stop at brush's max face
            }
        }

        // Recompute proposed AABB after clamping for subsequent checks
        proposedAABB = AABB::createFromMinMax(mins, maxs);
    }
}

MouseTool::Result CameraBrushCreatorTool::onMouseDown(Event& ev)
{
    try
    {
        if (GlobalClipper().clipMode() || GlobalMapModule().getEditMode() == IMap::EditMode::Merge ||
            GlobalOrthoViewManager().polygonMode())
        {
            return Result::Ignored; // no brush creation in clip, merge or polygon mode
        }

        // We only operate on camera view events
        CameraMouseToolEvent& camEvent = dynamic_cast<CameraMouseToolEvent&>(ev);

        // Handle double-click for face selection regardless of current selection state
        if (camEvent.isDoubleClick())
        {
            _brush.reset();
            _hasMoved = false;
            _isDoubleClick = true;
            _startDevicePos = ev.getDevicePosition();
            return Result::Activated;
        }

        _isDoubleClick = false;

        // Only start brush creation if nothing is selected (neither primitives nor components)
        // This allows the manipulate tool to handle clicks when only components (e.g., faces) are selected
        if (GlobalSelectionSystem().countSelected() == 0 &&
            GlobalSelectionSystem().countSelectedComponents() == 0)
        {
            _brush.reset();
            _hasMoved = false;
            _altWasHeld = false;
            _constructOnSurface = false;

            // Get the work zone to determine the fallback construction plane height
            const selection::WorkZone& wz = GlobalSelectionSystem().getWorkZone();
            _planeHeight = wz.bounds.getOrigin().z();

            // Store the starting device position for screen-based height calculation
            _startDevicePos = ev.getDevicePosition();
            _heightRefDevicePos = ev.getDevicePosition();

            // Store view dimensions for overlay rendering
            _viewWidth = ev.getInteractiveView().getDeviceWidth();
            _viewHeight = ev.getInteractiveView().getDeviceHeight();

            // Try surface detection first
            Vector3 hitPoint, hitNormal;
            Ray ray = calculateRayForDevicePoint(camEvent.getView(), ev.getDevicePosition());

            if (findSurfaceUnderCursor(camEvent, ev.getDevicePosition(), hitPoint, hitNormal))
            {
                // Check if the face is axis-aligned (normal along X, Y, or Z)
                bool axisAligned = false;
                for (int axis = 0; axis < 3; axis++)
                {
                    if (std::abs(std::abs(hitNormal[axis]) - 1.0) < 0.01)
                    {
                        axisAligned = true;
                        break;
                    }
                }

                if (axisAligned)
                {
                    _constructOnSurface = true;
                    _surfaceNormal = hitNormal;

                    // Build construction plane from the hit face
                    double planeDist = hitNormal.dot(hitPoint);
                    _constructionPlane = Plane3(hitNormal, planeDist);

                    // Project ray onto the construction plane for start position
                    _startPos = ray.origin + ray.direction * ray.getDistance(_constructionPlane);
                    snapToGrid(_startPos);

                    // Snap the normal axis to the face's exact coordinate
                    for (int axis = 0; axis < 3; axis++)
                    {
                        if (std::abs(hitNormal[axis]) > 0.9)
                        {
                            _startPos[axis] = float_snapped(hitPoint[axis],
                                GlobalGrid().getGridSize());
                            break;
                        }
                    }
                }
                else
                {
                    // Non-axis-aligned face: fall back to horizontal plane at hit point's Z
                    _planeHeight = float_snapped(hitPoint.z(), GlobalGrid().getGridSize());
                    _startPos = getWorldPosOnPlane(ray, _planeHeight);
                    snapToGrid(_startPos);
                }
            }
            else
            {
                // No surface hit: fall back to original horizontal plane behavior
                _startPos = getWorldPosOnPlane(ray, _planeHeight);
                snapToGrid(_startPos);
            }

            _lastEndPos = _startPos;

            // Cache nearby brushes for snapping during drag
            double gridSize = GlobalGrid().getGridSize();
            double searchRadius = std::max(gridSize * 20.0, 512.0);
            cacheNearbyBrushes(_startPos, searchRadius);

            GlobalUndoSystem().start();

            return Result::Activated;
        }
    }
    catch (std::bad_cast&)
    {
    }

    return Result::Ignored; // not handled
}

MouseTool::Result CameraBrushCreatorTool::onMouseMove(Event& ev)
{
    try
    {
        // We only operate on camera view events
        CameraMouseToolEvent& camEvent = dynamic_cast<CameraMouseToolEvent&>(ev);

        _hasMoved = true;

        // If this is a double-click operation, just track that we moved (for face selection)
        if (_isDoubleClick)
        {
            return Result::Continued;
        }

        // Calculate current world position on the construction plane
        Ray ray = calculateRayForDevicePoint(camEvent.getView(), ev.getDevicePosition());
        Vector3 endPos;
        if (_constructOnSurface)
        {
            double dist = ray.getDistance(_constructionPlane);
            // If ray is nearly parallel or pointing away from the plane,
            // fall back to projecting along the dominant construction axes
            if (dist <= 0 || !std::isfinite(dist))
                endPos = _lastEndPos; // keep previous valid position
            else
                endPos = ray.origin + ray.direction * dist;
        }
        else
        {
            endPos = getWorldPosOnPlane(ray, _planeHeight);
        }
        snapToGrid(endPos);

        // Get current modifier state using configured modifiers
        unsigned int squareModifier = GlobalCamera().getBrushSquareModifierFlags();
        unsigned int heightModifier = GlobalCamera().getBrushHeightModifierFlags();

        bool shiftHeld = areModifiersHeld(squareModifier);
        bool altHeld = areModifiersHeld(heightModifier);

        double gridSize = GlobalGrid().getGridSize();

        // Calculate brush bounds
        Vector3 mins = _startPos;
        Vector3 maxs = endPos;

        // If Alt is held (without Shift), only change height - keep X and Y from the previous end position
        if (altHeld && !shiftHeld)
        {
            // Detect when Alt is first pressed and reset the height reference point
            if (!_altWasHeld)
            {
                _heightRefDevicePos = ev.getDevicePosition();
                _altWasHeld = true;
            }

            // Map vertical screen movement to height change
            // Device coords are normalized (-1 to 1), Y increases upward in NDC
            // Moving mouse UP should INCREASE height
            double screenYDelta = ev.getDevicePosition().y() - _heightRefDevicePos.y();

            // Scale relative to view height for reasonable sensitivity
            // Positive delta (mouse up) = more height
            double heightChange = screenYDelta * 100.0 * gridSize;

            // Use the previously established X and Y
            maxs.x() = _lastEndPos.x();
            maxs.y() = _lastEndPos.y();

            // Height is grid size + the delta, but never less than grid size
            double height = gridSize + heightChange;
            if (height < gridSize) height = gridSize;

            maxs.z() = mins.z() + height;
            snapToGrid(maxs);
        }
        else
        {
            // Reset Alt tracking when Alt is released
            _altWasHeld = false;

            // Set the thin dimension along the surface normal (or Z by default)
            if (_constructOnSurface)
            {
                for (int axis = 0; axis < 3; axis++)
                {
                    if (std::abs(_surfaceNormal[axis]) > 0.9)
                    {
                        double sign = _surfaceNormal[axis] > 0 ? 1.0 : -1.0;
                        maxs[axis] = mins[axis] + gridSize * sign;
                        break;
                    }
                }
            }
            else
            {
                maxs.z() = mins.z() + gridSize;
            }

            // Store for Alt modifier later (remember the X/Y position)
            _lastEndPos = maxs;
        }

        // Apply constraint modifiers
        applyConstraints(mins, maxs, shiftHeld, altHeld);

        // Ensure mins < maxs for each axis
        for (int i = 0; i < 3; i++)
        {
            if (mins[i] > maxs[i])
            {
                std::swap(mins[i], maxs[i]);
            }
        }

        // Snap dragged edges to nearby brush faces and prevent overlap.
        // Skip the thin axis (programmatically set, not user-dragged).
        int thinAxis = 2; // default: Z
        if (_constructOnSurface)
        {
            for (int a = 0; a < 3; a++)
            {
                if (std::abs(_surfaceNormal[a]) > 0.9) { thinAxis = a; break; }
            }
        }

        double snapThreshold = GlobalGrid().getGridSize() * 2.0;
        for (int axis = 0; axis < 3; axis++)
        {
            if (axis == thinAxis) continue;

            // Only snap the dragged edge (away from _startPos), not the anchor
            if (_startPos[axis] <= mins[axis])
                maxs[axis] = snapToNearbyFace(maxs[axis], axis, snapThreshold);
            else
                mins[axis] = snapToNearbyFace(mins[axis], axis, snapThreshold);
        }
        clampToPreventOverlap(_startPos, mins, maxs);

        // Check for degenerate brush
        for (int i = 0; i < 3; i++)
        {
            if (mins[i] == maxs[i])
            {
                return Result::Continued; // don't create a degenerate brush
            }
        }

        if (!_brush)
        {
            // Create a new brush
            _brush = GlobalBrushCreator().createBrush();

            if (_brush)
            {
                // Insert the brush into worldspawn
                auto worldspawn = GlobalMapModule().findOrInsertWorldspawn();
                scene::addNodeToContainer(_brush, worldspawn);
            }

            // Make sure the brush is selected
            Node_setSelected(_brush, true);
        }

        // Check brush validity
        if (!_brush->getParent() || GlobalSelectionSystem().countSelected() == 0)
        {
            _brush.reset();
            return Result::Finished;
        }

        // Dispatch the command to resize the brush
        GlobalCommandSystem().executeCommand(
            "ResizeSelectedBrushesToBounds",
            {mins, maxs, GlobalTextureBrowser().getSelectedShader()}
        );
    }
    catch (cmd::ExecutionNotPossible&)
    {
        return Result::Ignored;
    }
    catch (std::bad_cast&)
    {
        return Result::Ignored;
    }

    return Result::Continued;
}

MouseTool::Result CameraBrushCreatorTool::onMouseUp(Event& ev)
{
    try
    {
        // We only operate on camera view events
        CameraMouseToolEvent& camEvent = dynamic_cast<CameraMouseToolEvent&>(ev);

        if (_brush)
        {
            // A brush was created during dragging, finish the undo operation
            GlobalUndoSystem().finish("brushDragNew3D");
            _brush.reset();
        }
        else if (!_hasMoved)
        {
            // No brush was created - user just clicked without dragging
            // Cancel the undo operation and perform a selection instead
            GlobalUndoSystem().cancel();

            // Perform point selection at the click position
            float selectEpsilon = registry::getValue<float>("user/ui/selectionEpsilon");
            Vector2 epsilon(
                selectEpsilon / ev.getInteractiveView().getDeviceWidth(),
                selectEpsilon / ev.getInteractiveView().getDeviceHeight()
            );

            render::View view(ev.getInteractiveView().getVolumeTest());
            render::View scissored(view);
            ConstructSelectionTest(scissored,
                selection::Rectangle::ConstructFromPoint(ev.getDevicePosition(), epsilon));

            SelectionVolume volume(scissored);

            if (_isDoubleClick)
            {
                // Double-click: switch to component/face mode and select the face
                // First, select the primitive (brush) under the cursor so that component mode
                // doesn't immediately exit (checkComponentModeSelectionMode checks countSelected)
                GlobalSelectionSystem().selectPoint(volume, selection::SelectionSystem::eToggle, false);

                // Now switch to component/face mode
                GlobalSelectionSystem().SetComponentMode(selection::ComponentSelectionMode::Face);
                GlobalSelectionSystem().setSelectionMode(selection::SelectionMode::Component);

                // Select the face component
                GlobalSelectionSystem().selectPoint(volume, selection::SelectionSystem::eToggle, true);
            }
            else
            {
                // Single click: regular object selection
                GlobalSelectionSystem().selectPoint(volume, selection::SelectionSystem::eToggle, false);
            }

            ev.getInteractiveView().queueDraw();
        }
        else
        {
            // User dragged but brush creation failed
            GlobalUndoSystem().cancel();
        }

        return Result::Finished;
    }
    catch (std::bad_cast&)
    {
        return Result::Ignored;
    }
}

CameraBrushCreatorTool::Result CameraBrushCreatorTool::onCancel(IInteractiveView& view)
{
    if (_brush)
    {
        // We have a WIP brush object, kill it
        scene::removeNodeFromParent(_brush);
        GlobalUndoSystem().cancel();

        _brush.reset();
    }

    return Result::Finished;
}

void CameraBrushCreatorTool::onMouseCaptureLost(IInteractiveView& view)
{
    onCancel(view);
}

unsigned int CameraBrushCreatorTool::getPointerMode()
{
    return PointerMode::Capture;
}

unsigned int CameraBrushCreatorTool::getRefreshMode()
{
    return RefreshMode::Force | RefreshMode::AllViews;
}

void CameraBrushCreatorTool::renderOverlay()
{
    // Only show hint when actively creating a brush
    if (!_brush) return;

    // Build modifier hint dynamically from configured modifiers
    unsigned int squareModifier = GlobalCamera().getBrushSquareModifierFlags();
    unsigned int heightModifier = GlobalCamera().getBrushHeightModifierFlags();

    std::string squareModStr = wxutil::Modifier::GetLocalisedModifierString(squareModifier);
    std::string heightModStr = wxutil::Modifier::GetLocalisedModifierString(heightModifier);
    std::string cubeModStr = wxutil::Modifier::GetLocalisedModifierString(squareModifier | heightModifier);

    std::string hint;
    if (_constructOnSurface)
        hint = _("Drawing on surface | ");

    hint += squareModStr + _(": Square (X=Y)  |  ") +
            heightModStr + _(": Height only  |  ") +
            cubeModStr + _(": Cube (X=Y=Z)");

    // Position in lower-left corner with padding to avoid FPS/debug timers
    // The stats line is at (4, height-4), so we go 40 pixels below that
    float fontHeight = static_cast<float>(GlobalOpenGL().getFontHeight());
    float yPos = static_cast<float>(_viewHeight) - 4.0f - fontHeight * 3.0f;

    glColor3f(1.0f, 1.0f, 1.0f);
    glRasterPos3f(4.0f, yPos, 0.0f);
    GlobalOpenGL().drawString(hint);
}

} // namespace
