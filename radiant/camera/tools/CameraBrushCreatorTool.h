#pragma once

#include "imousetool.h"
#include "icameraview.h"
#include "inode.h"
#include "math/Vector3.h"
#include "math/Vector2.h"
#include "math/Ray.h"
#include "math/Plane3.h"
#include "math/AABB.h"

#include <vector>

namespace ui
{

class CameraMouseToolEvent;

/**
 * This tool provides the drag-to-create-a-brush functionality for the 3D camera view.
 *
 * When nothing is selected, clicking and dragging creates a new brush.
 * The brush dimensions are determined by:
 * - Start point: ray cast from initial click position to a horizontal plane
 * - End point: ray cast from current mouse position to the same plane
 * - Height: current grid size by default
 *
 * Modifier keys during drag:
 * - Shift: constrain X = Y (square base)
 * - Shift+Alt: constrain X = Y = Z (cube)
 * - Alt: change height only (keep X and Y fixed)
 *
 * If the user clicks without dragging, a point selection is performed instead.
 */
class CameraBrushCreatorTool :
    public MouseTool
{
private:
    scene::INodePtr _brush;
    Vector3 _startPos;          // World position where drag started
    double _planeHeight;        // Z height of the construction plane
    bool _hasMoved;             // True if mouse moved during drag
    Vector3 _lastEndPos;        // Last calculated end position (for Alt modifier)
    Vector2 _startDevicePos;    // Device position where drag started (for screen-based height)
    Vector2 _heightRefDevicePos; // Device position when Alt was pressed (reference for height)
    bool _altWasHeld;           // Track Alt state to detect when it's first pressed
    bool _isDoubleClick;        // True if this interaction started with a double-click
    int _viewWidth;             // View width for overlay rendering
    int _viewHeight;            // View height for overlay rendering

    // Cached nearby brush AABBs for snapping during drag (populated on mouseDown)
    struct NearbyBrush { AABB aabb; };
    std::vector<NearbyBrush> _nearbyBrushes;

    // Construction plane for this drag (either from clicked brush face or default horizontal)
    Plane3 _constructionPlane;
    bool _constructOnSurface = false;   // true if user clicked on a brush face
    Vector3 _surfaceNormal;             // face normal of clicked surface

    // Calculate a ray from the camera through the given device point
    Ray calculateRayForDevicePoint(camera::ICameraView& camView, const Vector2& devicePoint) const;

    // Get world position by intersecting ray with horizontal plane at given Z
    Vector3 getWorldPosOnPlane(const Ray& ray, double planeZ) const;

    // Apply grid snapping to a position
    void snapToGrid(Vector3& pos) const;

    // Apply constraint modifiers to the brush dimensions
    void applyConstraints(Vector3& mins, Vector3& maxs, bool shiftHeld, bool altHeld) const;

    // Surface detection and snapping helpers
    bool findSurfaceUnderCursor(CameraMouseToolEvent& camEvent, const Vector2& devicePos,
                                Vector3& outPoint, Vector3& outNormal);
    void cacheNearbyBrushes(const Vector3& center, double radius);
    double snapToNearbyFace(double value, int axis, double snapThreshold) const;
    void clampToPreventOverlap(const Vector3& anchor, Vector3& mins, Vector3& maxs) const;

public:
    const std::string& getName() override;
    const std::string& getDisplayName() override;

    Result onMouseDown(Event& ev) override;
    Result onMouseMove(Event& ev) override;
    Result onMouseUp(Event& ev) override;

    unsigned int getPointerMode() override;
    unsigned int getRefreshMode() override;
    Result onCancel(IInteractiveView& view) override;
    void onMouseCaptureLost(IInteractiveView& view) override;
    void renderOverlay() override;
};

} // namespace
