#include "PolygonTool.h"

#include "i18n.h"
#include "igrid.h"
#include "ibrush.h"
#include "imap.h"
#include "iundo.h"
#include "iselection.h"
#include "icolourscheme.h"
#include "ui/imainframe.h"
#include "ui/idialogmanager.h"
#include "scenelib.h"
#include "selectionlib.h"
#include "math/FloatTools.h"
#include "math/Plane3.h"
#include "XYMouseToolEvent.h"
#include "camera/tools/CameraMouseToolEvent.h"
#include "camera/tools/FaceIntersectionFinder.h"
#include "icameraview.h"
#include "iscenegraph.h"
#include "selection/SelectionVolume.h"
#include "igl.h"
#include "settings/RenderingQualitySettings.h"
#include "ui/texturebrowser/TextureBrowserPanel.h"
#include "ui/texturebrowser/TextureBrowserManager.h"
#include "../GlobalXYWnd.h"

namespace ui
{

PolygonTool::PolygonTool() :
    _viewType(OrthoOrientation::XY),
    _viewScale(1.0f),
    _isDrawing(false),
    _isCameraMode(false),
    _pointsRenderable(_renderVertices),
    _lineRenderable(_renderVertices)
{}

const std::string& PolygonTool::getName()
{
    static std::string name("PolygonTool");

    return name;
}

const std::string& PolygonTool::getDisplayName()
{
    static std::string displayName(_("Draw Polygon Brush"));

    return displayName;
}

MouseTool::Result PolygonTool::onMouseDown(Event& ev)
{
    // Try ortho view first
    try {
        XYMouseToolEvent& xyEvent = dynamic_cast<XYMouseToolEvent&>(ev);

        // If already drawing in camera mode, ignore ortho events
        if (_isDrawing && _isCameraMode)
        {
            return Result::Ignored;
        }

        if (!GlobalXYWnd().polygonMode())
        {
            return Result::Finished;
        }

        Vector3 point = xyEvent.getWorldPos();
        xyEvent.getView().snapToGrid(point);

        if (_points.empty())
        {
            _viewType = xyEvent.getViewType();
            _viewScale = xyEvent.getScale();
            _isDrawing = true;
            _isCameraMode = false;

            // Register Return key filter to complete polygon
            if (!_returnKeyFilter)
            {
                _returnKeyFilter = std::make_shared<wxutil::KeyEventFilter>(
                    WXK_RETURN,
                    std::bind(&PolygonTool::handleReturnKey, this));
                _spaceKeyFilter = std::make_shared<wxutil::KeyEventFilter>(
                    WXK_SPACE,
                    std::bind(&PolygonTool::handleReturnKey, this));
            }
        }

        _viewScale = xyEvent.getScale();

        // Check if we should close the polygon (clicking near first point)
        if (_points.size() >= MIN_POLYGON_POINTS && isNearFirstPoint(point))
        {
            finishPolygon();
            return Result::Finished;
        }

        // Add the point
        addPoint(point);
        _currentMousePos = point;

        updateRenderables();
        GlobalMainFrame().updateAllWindows();

        return Result::Activated;
    }
    catch (std::bad_cast&)
    {
    }

    // Try camera view
    try {
        CameraMouseToolEvent& camEvent = dynamic_cast<CameraMouseToolEvent&>(ev);

        // If already drawing in ortho mode, ignore camera events
        if (_isDrawing && !_isCameraMode)
        {
            return Result::Ignored;
        }

        if (!GlobalXYWnd().polygonMode())
        {
            return Result::Finished;
        }

        // Store camera matrices for overlay rendering
        _cameraModelView = camEvent.getView().getModelView();
        _cameraProjection = camEvent.getView().getProjection();
        _cameraWidth = static_cast<int>(ev.getInteractiveView().getDeviceWidth());
        _cameraHeight = static_cast<int>(ev.getInteractiveView().getDeviceHeight());

        Ray ray = calculateRayForDevicePoint(camEvent.getView(), ev.getDevicePosition());

        if (_points.empty())
        {
            // First click: establish construction plane
            Vector3 hitPoint, hitNormal;

            if (findSurfaceUnderCursor(camEvent, hitPoint, hitNormal))
            {
                // Determine if the hit normal is axis-aligned
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
                    // Map normal to ortho orientation
                    if (std::abs(hitNormal.z()) > 0.9)
                        _viewType = OrthoOrientation::XY;
                    else if (std::abs(hitNormal.x()) > 0.9)
                        _viewType = OrthoOrientation::YZ;
                    else
                        _viewType = OrthoOrientation::XZ;

                    _constructionNormal = hitNormal;
                    double planeDist = hitNormal.dot(hitPoint);
                    _constructionPlane = Plane3(hitNormal, planeDist);
                }
                else
                {
                    // Non-axis-aligned: fall back to horizontal XY plane at hit Z
                    _viewType = OrthoOrientation::XY;
                    double hitZ = float_snapped(hitPoint.z(), GlobalGrid().getGridSize());
                    _constructionNormal = Vector3(0, 0, 1);
                    _constructionPlane = Plane3(_constructionNormal, hitZ);
                }
            }
            else
            {
                // No surface hit: horizontal XY plane at workzone Z
                _viewType = OrthoOrientation::XY;
                const selection::WorkZone& wz = GlobalSelectionSystem().getWorkZone();
                double planeZ = float_snapped(wz.bounds.getOrigin().z(), GlobalGrid().getGridSize());
                _constructionNormal = Vector3(0, 0, 1);
                _constructionPlane = Plane3(_constructionNormal, planeZ);
            }

            _isCameraMode = true;
            _isDrawing = true;

            if (!_returnKeyFilter)
            {
                _returnKeyFilter = std::make_shared<wxutil::KeyEventFilter>(
                    WXK_RETURN,
                    std::bind(&PolygonTool::handleReturnKey, this));
                _spaceKeyFilter = std::make_shared<wxutil::KeyEventFilter>(
                    WXK_SPACE,
                    std::bind(&PolygonTool::handleReturnKey, this));
            }
        }

        // Project ray onto construction plane
        Vector3 point = projectOntoConstructionPlane(ray);
        double gridSize = GlobalGrid().getGridSize();
        point.x() = float_snapped(point.x(), gridSize);
        point.y() = float_snapped(point.y(), gridSize);
        point.z() = float_snapped(point.z(), gridSize);

        // Check if we should close the polygon
        if (_points.size() >= MIN_POLYGON_POINTS && isNearFirstPoint(point))
        {
            finishPolygon();
            return Result::Finished;
        }

        addPoint(point);
        _currentMousePos = point;

        updateRenderables();
        GlobalMainFrame().updateAllWindows();

        return Result::Activated;
    }
    catch (std::bad_cast&)
    {
    }

    return Result::Ignored;
}

MouseTool::Result PolygonTool::onMouseMove(Event& ev)
{
    // Try ortho view
    try
    {
        XYMouseToolEvent& xyEvent = dynamic_cast<XYMouseToolEvent&>(ev);

        if (_isDrawing && !_isCameraMode && !_points.empty())
        {
            _currentMousePos = xyEvent.getWorldPos();
            xyEvent.getView().snapToGrid(_currentMousePos);
            _viewScale = xyEvent.getScale();

            updateRenderables();

            return Result::Continued;
        }
    }
    catch (std::bad_cast&)
    {
    }

    // Try camera view
    try
    {
        CameraMouseToolEvent& camEvent = dynamic_cast<CameraMouseToolEvent&>(ev);

        if (_isDrawing && _isCameraMode && !_points.empty())
        {
            // Update camera matrices for overlay rendering
            _cameraModelView = camEvent.getView().getModelView();
            _cameraProjection = camEvent.getView().getProjection();
            _cameraWidth = static_cast<int>(ev.getInteractiveView().getDeviceWidth());
            _cameraHeight = static_cast<int>(ev.getInteractiveView().getDeviceHeight());

            Ray ray = calculateRayForDevicePoint(camEvent.getView(), ev.getDevicePosition());
            _currentMousePos = projectOntoConstructionPlane(ray);

            double gridSize = GlobalGrid().getGridSize();
            _currentMousePos.x() = float_snapped(_currentMousePos.x(), gridSize);
            _currentMousePos.y() = float_snapped(_currentMousePos.y(), gridSize);
            _currentMousePos.z() = float_snapped(_currentMousePos.z(), gridSize);

            updateRenderables();

            return Result::Continued;
        }
    }
    catch (std::bad_cast&)
    {
    }

    return Result::Ignored;
}

MouseTool::Result PolygonTool::onMouseUp(Event& ev)
{
    try
    {
        dynamic_cast<XYMouseToolEvent&>(ev);
        if (_isDrawing && !_isCameraMode)
            return Result::Continued;
    }
    catch (std::bad_cast&)
    {
    }

    try
    {
        dynamic_cast<CameraMouseToolEvent&>(ev);
        if (_isDrawing && _isCameraMode)
            return Result::Continued;
    }
    catch (std::bad_cast&)
    {
    }

    return Result::Ignored;
}

MouseTool::Result PolygonTool::onCancel(IInteractiveView& view)
{
    reset();
    GlobalMainFrame().updateAllWindows();

    return Result::Finished;
}

void PolygonTool::onMouseCaptureLost(IInteractiveView& view)
{
    // Only reset when explicitly cancelled via onCancel or polygon completed
}

bool PolygonTool::alwaysReceivesMoveEvents()
{
    return true;
}

unsigned int PolygonTool::getPointerMode()
{
    return PointerMode::Normal;
}

unsigned int PolygonTool::getRefreshMode()
{
    return RefreshMode::Force | RefreshMode::AllViews;
}

void PolygonTool::finishPolygonIfReady()
{
    if (_points.size() >= MIN_POLYGON_POINTS)
    {
        finishPolygon();
    }
}

void PolygonTool::cancelPolygonDrawing()
{
    reset();
    GlobalMainFrame().updateAllWindows();
}

bool PolygonTool::hasActivePolygon() const
{
    return _isDrawing && !_points.empty();
}

void PolygonTool::ensureShaders(RenderSystem& renderSystem)
{
    if (!_wireShader)
    {
        Vector3 themeColour = GlobalColourSchemeManager().getColour("drag_selection");
        _colour = Vector4(themeColour.x(), themeColour.y(), themeColour.z(), 1.0f);
        _wireShader = renderSystem.capture(ColourShaderType::OrthoviewSolid, _colour);
    }

    if (!_pointShader)
    {
        _pointShader = renderSystem.capture(BuiltInShaderType::Point);
    }
}

void PolygonTool::updateRenderables()
{
    _renderVertices.clear();

    for (const auto& pt : _points)
    {
        _renderVertices.push_back(Vertex3(pt));
    }

    // Add current mouse position for preview line
    if (_isDrawing && !_points.empty())
    {
        _renderVertices.push_back(Vertex3(_currentMousePos));

        // Close the loop preview to first point
        _renderVertices.push_back(Vertex3(_points[0]));
    }

    _pointsRenderable.queueUpdate();
    _lineRenderable.queueUpdate();
}

void PolygonTool::render(RenderSystem& renderSystem, IRenderableCollector& collector, const VolumeTest& volume)
{
    if (_points.empty())
    {
        return;
    }

    ensureShaders(renderSystem);

    _pointsRenderable.update(_pointShader);
    _lineRenderable.update(_wireShader);
}

void PolygonTool::renderOverlay()
{
    if (!_isCameraMode || _points.empty() || _cameraWidth == 0) return;

    // Replace the 2D ortho projection with the stored 3D camera matrices
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadMatrixd(_cameraProjection);

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadMatrixd(_cameraModelView);

    // Ensure color is set
    if (_colour.w() == 0)
    {
        Vector3 c = GlobalColourSchemeManager().getColour("drag_selection");
        _colour = Vector4(c.x(), c.y(), c.z(), 1.0f);
    }

    glDisable(GL_LIGHTING);
    glDisable(GL_TEXTURE_2D);
    glEnable(GL_DEPTH_TEST);
    GlobalRenderingQualitySettings().applyLineSmoothing();

    // Draw polygon outline
    glColor3f(static_cast<float>(_colour.x()), static_cast<float>(_colour.y()), static_cast<float>(_colour.z()));
    glLineWidth(2.0f);

    glBegin(GL_LINE_STRIP);
    for (const auto& pt : _points)
        glVertex3d(pt.x(), pt.y(), pt.z());
    glVertex3d(_currentMousePos.x(), _currentMousePos.y(), _currentMousePos.z());
    glVertex3d(_points[0].x(), _points[0].y(), _points[0].z());
    glEnd();

    // Draw vertex points
    glPointSize(6.0f);

    glBegin(GL_POINTS);
    for (const auto& pt : _points)
        glVertex3d(pt.x(), pt.y(), pt.z());
    glEnd();

    // Restore GL state
    glPointSize(1.0f);
    glLineWidth(1.0f);
    GlobalRenderingQualitySettings().disableSmoothing();
    glEnable(GL_LIGHTING);

    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
}

bool PolygonTool::isNearFirstPoint(const Vector3& point) const
{
    if (_points.empty())
    {
        return false;
    }

    int axis1, axis2;
    getViewAxes(axis1, axis2);

    if (_isCameraMode)
    {
        // In camera mode, use world-space distance threshold
        double gridSize = GlobalGrid().getGridSize();
        double threshold = gridSize * 0.5;
        double dx = point[axis1] - _points[0][axis1];
        double dy = point[axis2] - _points[0][axis2];
        double distanceSq = dx * dx + dy * dy;
        return distanceSq < (threshold * threshold);
    }

    double dx = (point[axis1] - _points[0][axis1]) * _viewScale;
    double dy = (point[axis2] - _points[0][axis2]) * _viewScale;
    double distanceSq = dx * dx + dy * dy;

    return distanceSq < (CLOSE_DISTANCE_PIXELS * CLOSE_DISTANCE_PIXELS);
}

bool PolygonTool::isConvex() const
{
    if (_points.size() < MIN_POLYGON_POINTS)
    {
        return false;
    }

    int axis1, axis2;
    getViewAxes(axis1, axis2);

    int sign = 0;
    std::size_t n = _points.size();

    for (std::size_t i = 0; i < n; ++i)
    {
        std::size_t j = (i + 1) % n;
        std::size_t k = (i + 2) % n;

        double v1x = _points[j][axis1] - _points[i][axis1];
        double v1y = _points[j][axis2] - _points[i][axis2];
        double v2x = _points[k][axis1] - _points[j][axis1];
        double v2y = _points[k][axis2] - _points[j][axis2];

        double cross = v1x * v2y - v1y * v2x;

        if (std::abs(cross) > 0.001)
        {
            int newSign = (cross > 0) ? 1 : -1;
            if (sign == 0)
            {
                sign = newSign;
            }
            else if (sign != newSign)
            {
                return false;
            }
        }
    }

    return true;
}

void PolygonTool::finishPolygon()
{
    if (_points.size() < MIN_POLYGON_POINTS)
    {
        reset();
        return;
    }

    if (!isConvex())
    {
        GlobalDialogManager().createMessageBox(
            _("Polygon Tool"),
            _("Cannot create brush: polygon is not convex. Only convex shapes are supported."),
            ui::IDialog::MessageType::MESSAGE_ERROR
        )->run();
        reset();
        return;
    }

    auto brushNode = createBrushFromPolygon();
    if (brushNode)
    {
        UndoableCommand cmd("polygonBrush");

        auto worldspawn = GlobalMapModule().findOrInsertWorldspawn();
        scene::addNodeToContainer(brushNode, worldspawn);

        GlobalSelectionSystem().setSelectedAll(false);
        Node_setSelected(brushNode, true);
    }

    reset();

    GlobalXYWnd().setPolygonMode(false);
    GlobalMainFrame().updateAllWindows();
}

scene::INodePtr PolygonTool::createBrushFromPolygon()
{
    if (_points.size() < MIN_POLYGON_POINTS)
    {
        return scene::INodePtr();
    }

    double minDepth, maxDepth;
    getDepthRange(minDepth, maxDepth);

    int depthAxis = getExtrusionAxis();
    int axis1, axis2;
    getViewAxes(axis1, axis2);

    auto brushNode = GlobalBrushCreator().createBrush();
    auto* brush = Node_getIBrush(brushNode);

    if (!brush)
    {
        return scene::INodePtr();
    }

    std::string shader = GlobalTextureBrowser().getSelectedShader();
    if (shader.empty())
    {
        shader = "_default";
    }

    brush->clear();

    // Create top cap using plane normal and distance
    {
        Vector3 normal(0, 0, 0);
        normal[depthAxis] = 1.0;
        Plane3 plane(normal, maxDepth);
        IFace& face = brush->addFace(plane);
        face.setShader(shader);
    }

    // Create bottom cap
    {
        Vector3 normal(0, 0, 0);
        normal[depthAxis] = -1.0;
        Plane3 plane(normal, -minDepth);
        IFace& face = brush->addFace(plane);
        face.setShader(shader);
    }

    // Determine polygon winding direction
    double windingSum = 0;
    for (std::size_t i = 0; i < _points.size(); ++i)
    {
        std::size_t next = (i + 1) % _points.size();
        windingSum += (_points[next][axis1] - _points[i][axis1]) *
                      (_points[next][axis2] + _points[i][axis2]);
    }
    double windingSign = (windingSum > 0) ? -1.0 : 1.0;

    // Create side faces
    for (std::size_t i = 0; i < _points.size(); ++i)
    {
        std::size_t next = (i + 1) % _points.size();

        double edgeX = _points[next][axis1] - _points[i][axis1];
        double edgeY = _points[next][axis2] - _points[i][axis2];

        // Apply winding sign to ensure outward-facing normal
        double normalX = edgeY * windingSign;
        double normalY = -edgeX * windingSign;

        // Normalize
        double len = std::sqrt(normalX * normalX + normalY * normalY);
        if (len < 0.0001)
        {
            continue;
        }

        normalX /= len;
        normalY /= len;

        // Create 3D normal vector
        Vector3 normal(0, 0, 0);
        normal[axis1] = normalX;
        normal[axis2] = normalY;

        // Distance from origin to plane
        double dist = normal[axis1] * _points[i][axis1] + normal[axis2] * _points[i][axis2];

        Plane3 plane(normal, dist);
        IFace& face = brush->addFace(plane);
        face.setShader(shader);
    }

    // Evaluate the brush geometry
    brush->evaluateBRep();

    return brushNode;
}

void PolygonTool::reset()
{
    _points.clear();
    _renderVertices.clear();
    _currentMousePos = Vector3(0, 0, 0);
    _isDrawing = false;
    _isCameraMode = false;
    _pointsRenderable.clear();
    _lineRenderable.clear();
}

void PolygonTool::addPoint(const Vector3& point)
{
    Vector3 adjustedPoint = point;

    if (!_isCameraMode)
    {
        // In ortho mode, set the depth axis to workzone center
        int depthAxis = getExtrusionAxis();
        const selection::WorkZone& wz = GlobalSelectionSystem().getWorkZone();
        adjustedPoint[depthAxis] = float_snapped(
            (wz.min[depthAxis] + wz.max[depthAxis]) * 0.5,
            GlobalGrid().getGridSize()
        );
    }
    // In camera mode, the ray-plane intersection already has the correct depth

    _points.push_back(adjustedPoint);
}

int PolygonTool::getExtrusionAxis() const
{
    switch (_viewType)
    {
        case OrthoOrientation::XY: return 2;  // Z axis
        case OrthoOrientation::YZ: return 0;  // X axis
        case OrthoOrientation::XZ: return 1;  // Y axis
        default: return 2;
    }
}

void PolygonTool::getDepthRange(double& minDepth, double& maxDepth) const
{
    int depthAxis = getExtrusionAxis();
    double gridSize = GlobalGrid().getGridSize();

    if (_isCameraMode)
    {
        // In camera mode, extrude one grid unit from the construction surface
        // in the normal direction
        double surfaceDepth = _points.empty() ? 0 : _points[0][depthAxis];
        double sign = _constructionNormal[depthAxis];

        if (sign >= 0)
        {
            minDepth = surfaceDepth;
            maxDepth = surfaceDepth + gridSize;
        }
        else
        {
            minDepth = surfaceDepth - gridSize;
            maxDepth = surfaceDepth;
        }
        return;
    }

    const selection::WorkZone& wz = GlobalSelectionSystem().getWorkZone();

    // Check if workzone is valid
    if (wz.bounds.isValid() && wz.bounds.extents[depthAxis] > 0.01)
    {
        minDepth = float_snapped(wz.min[depthAxis], gridSize);
        maxDepth = float_snapped(wz.max[depthAxis], gridSize);
    }
    else
    {
        // Use a default depth centered at the polygon's depth coordinate
        double centerDepth = 0;
        if (!_points.empty())
        {
            centerDepth = _points[0][depthAxis];
        }
        minDepth = float_snapped(centerDepth - gridSize * 4, gridSize);
        maxDepth = float_snapped(centerDepth + gridSize * 4, gridSize);
    }

    // Ensure minimum depth
    if (maxDepth <= minDepth)
    {
        maxDepth = minDepth + gridSize;
    }
}

void PolygonTool::getViewAxes(int& axis1, int& axis2) const
{
    switch (_viewType)
    {
        case OrthoOrientation::XY:
            axis1 = 0;  // X
            axis2 = 1;  // Y
            break;
        case OrthoOrientation::YZ:
            axis1 = 1;  // Y
            axis2 = 2;  // Z
            break;
        case OrthoOrientation::XZ:
            axis1 = 0;  // X
            axis2 = 2;  // Z
            break;
        default:
            axis1 = 0;
            axis2 = 1;
            break;
    }
}

wxutil::KeyEventFilter::Result PolygonTool::handleReturnKey()
{
    if (_isDrawing && _points.size() >= MIN_POLYGON_POINTS)
    {
        finishPolygon();
        return wxutil::KeyEventFilter::Result::KeyProcessed;
    }

    // Always consume the key while polygon mode is active or drawing,
    // to prevent it leaking to shortcuts (e.g., Space → CloneSelection)
    if (GlobalXYWnd().polygonMode() || _isDrawing)
    {
        return wxutil::KeyEventFilter::Result::KeyProcessed;
    }

    return wxutil::KeyEventFilter::Result::KeyIgnored;
}

Ray PolygonTool::calculateRayForDevicePoint(camera::ICameraView& camView, const Vector2& devicePoint) const
{
    const Matrix4& modelView = camView.getModelView();
    const Matrix4& projection = camView.getProjection();

    Matrix4 viewProj = projection.getMultipliedBy(modelView);
    Matrix4 invViewProj = viewProj.getFullInverse();

    Vector3 nearPoint(devicePoint.x(), devicePoint.y(), -1.0);
    Vector3 farPoint(devicePoint.x(), devicePoint.y(), 1.0);

    Vector4 nearWorld4 = invViewProj.transform(Vector4(nearPoint.x(), nearPoint.y(), nearPoint.z(), 1.0));
    Vector4 farWorld4 = invViewProj.transform(Vector4(farPoint.x(), farPoint.y(), farPoint.z(), 1.0));

    Vector3 nearWorld = nearWorld4.getProjected();
    Vector3 farWorld = farWorld4.getProjected();

    return Ray::createForPoints(nearWorld, farWorld);
}

Vector3 PolygonTool::projectOntoConstructionPlane(const Ray& ray) const
{
    double distance = ray.getDistance(_constructionPlane);

    if (distance <= 0 || !std::isfinite(distance))
    {
        // Ray parallel or pointing away — return last known position
        return _currentMousePos;
    }

    return ray.origin + ray.direction * distance;
}

bool PolygonTool::findSurfaceUnderCursor(CameraMouseToolEvent& camEvent,
    Vector3& outPoint, Vector3& outNormal)
{
    SelectionTestPtr selectionTest = camEvent.getView().createSelectionTestForPoint(
        camEvent.getDevicePosition());
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

} // namespace ui
