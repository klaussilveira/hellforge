#pragma once

#include "imousetool.h"
#include "iorthoview.h"
#include "icameraview.h"
#include "inode.h"
#include "math/Vector3.h"
#include "math/Vector4.h"
#include "math/Vector2.h"
#include "math/Ray.h"
#include "math/Plane3.h"
#include "math/Matrix4.h"
#include "render.h"
#include "render/RenderableVertexArray.h"
#include "wxutil/event/KeyEventFilter.h"
#include <vector>

namespace ui
{

class XYMouseToolEvent;
class CameraMouseToolEvent;

/**
 * Tool for creating polygon-shaped brushes by adding points in ortho or camera view.
 */
class PolygonTool :
    public MouseTool
{
private:
    // Polygon vertices in world coordinates
    std::vector<Vector3> _points;

    // Current mouse position for preview line
    Vector3 _currentMousePos;

    // View type when polygon was started
    OrthoOrientation _viewType;

    // Scale factor for close-point detection
    float _viewScale;

    // Whether we're actively drawing
    bool _isDrawing;

    // Camera mode state
    bool _isCameraMode;
    Plane3 _constructionPlane;
    Vector3 _constructionNormal;

    // Stored camera matrices for overlay rendering
    Matrix4 _cameraModelView;
    Matrix4 _cameraProjection;
    int _cameraWidth = 0;
    int _cameraHeight = 0;

    // Rendering infrastructure
    std::vector<Vertex3> _renderVertices;
    render::RenderablePoints _pointsRenderable;
    render::RenderableLine _lineRenderable;
    ShaderPtr _pointShader;
    ShaderPtr _wireShader;
    Vector4 _colour;

    static constexpr double CLOSE_DISTANCE_PIXELS = 8.0;
    static constexpr std::size_t MIN_POLYGON_POINTS = 3;

    wxutil::KeyEventFilterPtr _returnKeyFilter;
    wxutil::KeyEventFilterPtr _spaceKeyFilter;

public:
    PolygonTool();

    const std::string& getName() override;
    const std::string& getDisplayName() override;

    Result onMouseDown(Event& ev) override;
    Result onMouseMove(Event& ev) override;
    Result onMouseUp(Event& ev) override;

    Result onCancel(IInteractiveView& view) override;
    void onMouseCaptureLost(IInteractiveView& view) override;

    bool alwaysReceivesMoveEvents() override;
    unsigned int getPointerMode() override;
    unsigned int getRefreshMode() override;

    // Render the polygon preview in world space (ortho views via render system)
    void render(RenderSystem& renderSystem, IRenderableCollector& collector, const VolumeTest& volume) override;

    // Render polygon preview in camera view via direct GL overlay
    void renderOverlay() override;

    void finishPolygonIfReady();
    void cancelPolygonDrawing();
    bool hasActivePolygon() const;

private:
    // Check if a point is near the first point (for closing polygon)
    bool isNearFirstPoint(const Vector3& point) const;

    // Check if the polygon is convex
    bool isConvex() const;

    // Finalize the polygon and create a brush
    void finishPolygon();

    // Convert polygon points to a brush
    scene::INodePtr createBrushFromPolygon();

    void reset();
    void addPoint(const Vector3& point);

    // Get the extrusion axis based on view type
    int getExtrusionAxis() const;

    // Get min/max depth for the brush from WorkZone
    void getDepthRange(double& minDepth, double& maxDepth) const;

    // Get the 2D axes for the current view orientation
    void getViewAxes(int& axis1, int& axis2) const;

    void ensureShaders(RenderSystem& renderSystem);
    void updateRenderables();
    wxutil::KeyEventFilter::Result handleReturnKey();

    // Camera mode helpers
    Ray calculateRayForDevicePoint(camera::ICameraView& camView, const Vector2& devicePoint) const;
    Vector3 projectOntoConstructionPlane(const Ray& ray) const;
    bool findSurfaceUnderCursor(CameraMouseToolEvent& camEvent, Vector3& outPoint, Vector3& outNormal);
};

} // namespace ui
