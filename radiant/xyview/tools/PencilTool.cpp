#include "PencilTool.h"

#include "i18n.h"
#include "iclipper.h"
#include "icolourscheme.h"
#include "ieclass.h"
#include "ientity.h"
#include "igl.h"
#include "igrid.h"
#include "imap.h"
#include "iscenegraph.h"
#include "iselection.h"
#include "iundo.h"
#include "ui/imainframe.h"
#include "scene/EntityNode.h"
#include "selectionlib.h"
#include "gamelib.h"
#include "math/FloatTools.h"
#include "math/Matrix4.h"
#include "math/Vector4.h"
#include "settings/RenderingQualitySettings.h"
#include "string/convert.h"
#include "StrokeGeometry.h"
#include "WallTool.h"
#include "XYMouseToolEvent.h"
#include "camera/tools/CameraMouseToolEvent.h"
#include "camera/tools/FaceIntersectionFinder.h"

#include <cmath>
#include <wx/utils.h>

namespace ui
{

namespace
{
    const char* const GKEY_DEFAULT_CURVE_ENTITY = "/defaults/defaultCurveEntity";
    const char* const GKEY_CURVE_CATMULLROM_KEY = "/defaults/curveCatmullRomKey";

    constexpr double SAMPLE_DISTANCE_PIXELS = 3.0;

    std::string serialiseCurve(const std::vector<Vector3>& points)
    {
        std::string value = string::to_string(points.size()) + " (";

        for (const auto& point : points)
        {
            value += " " + string::to_string(point.x()) + " " +
                     string::to_string(point.y()) + " " + string::to_string(point.z()) + " ";
        }

        return value + ")";
    }

    std::string getGameKey(const char* gameKey, const char* fallback)
    {
        auto value = game::current::getValue<std::string>(gameKey);

        return value.empty() ? fallback : value;
    }

    Vector3 getCentroid(const std::vector<Vector3>& points)
    {
        Vector3 sum(0, 0, 0);

        for (const auto& point : points)
        {
            sum += point;
        }

        return sum / static_cast<double>(points.size());
    }

    int getDepthAxis(OrthoOrientation viewType)
    {
        switch (viewType)
        {
            case OrthoOrientation::YZ: return 0;
            case OrthoOrientation::XZ: return 1;
            default: return 2;
        }
    }
}

PencilToolSettings& PencilToolSettings::Instance()
{
    static PencilToolSettings _instance;
    return _instance;
}

PencilTool::PencilTool() :
    _drawing(false),
    _cameraMode(false),
    _straightLine(false),
    _lastSamplePixel(0, 0),
    _lineRenderable(_renderVertices)
{}

const std::string& PencilTool::getName()
{
    static std::string name("PencilTool");
    return name;
}

const std::string& PencilTool::getDisplayName()
{
    static std::string displayName(_("Draw Curve"));
    return displayName;
}

MouseTool::Result PencilTool::onMouseDown(Event& ev)
{
    if (!PencilToolSettings::Instance().active || GlobalClipper().clipMode() ||
        GlobalMapModule().getEditMode() == IMap::EditMode::Merge ||
        GlobalOrthoViewManager().polygonMode() || WallToolSettings::Instance().active)
    {
        return Result::Ignored;
    }

    try
    {
        auto& xyEvent = dynamic_cast<XYMouseToolEvent&>(ev);

        beginStroke(getOrthoPoint(xyEvent), getPixelPosition(ev), false);

        return Result::Activated;
    }
    catch (std::bad_cast&)
    {
    }

    try
    {
        auto& camEvent = dynamic_cast<CameraMouseToolEvent&>(ev);

        Vector3 firstPoint;

        if (!setUpConstructionPlane(camEvent, firstPoint))
        {
            return Result::Ignored;
        }

        storeCameraMatrices(camEvent);

        beginStroke(firstPoint, getPixelPosition(ev), true);

        return Result::Activated;
    }
    catch (std::bad_cast&)
    {
    }

    return Result::Ignored;
}

MouseTool::Result PencilTool::onMouseMove(Event& ev)
{
    try
    {
        auto& xyEvent = dynamic_cast<XYMouseToolEvent&>(ev);

        if (_drawing && !_cameraMode)
        {
            addSample(getOrthoPoint(xyEvent), getPixelPosition(ev));

            return Result::Continued;
        }
    }
    catch (std::bad_cast&)
    {
    }

    try
    {
        auto& camEvent = dynamic_cast<CameraMouseToolEvent&>(ev);

        if (_drawing && _cameraMode)
        {
            storeCameraMatrices(camEvent);
            addSample(getCameraPoint(camEvent), getPixelPosition(ev));

            return Result::Continued;
        }
    }
    catch (std::bad_cast&)
    {
    }

    return Result::Ignored;
}

MouseTool::Result PencilTool::onMouseUp(Event& ev)
{
    if (!_drawing)
    {
        return Result::Ignored;
    }

    finishStroke();

    return Result::Finished;
}

MouseTool::Result PencilTool::onCancel(IInteractiveView& view)
{
    reset();
    GlobalMainFrame().updateAllWindows();

    return Result::Finished;
}

void PencilTool::onMouseCaptureLost(IInteractiveView& view)
{
    reset();
    GlobalMainFrame().updateAllWindows();
}

unsigned int PencilTool::getPointerMode()
{
    return PointerMode::Normal;
}

unsigned int PencilTool::getRefreshMode()
{
    return RefreshMode::Force | RefreshMode::ActiveView;
}

void PencilTool::render(RenderSystem& renderSystem, IRenderableCollector& collector, const VolumeTest& volume)
{
    if (_renderVertices.size() < 2)
    {
        return;
    }

    if (!_wireShader)
    {
        Vector3 colour = GlobalColourSchemeManager().getColour("drag_selection");
        _wireShader = renderSystem.capture(ColourShaderType::OrthoviewSolid,
            Vector4(colour.x(), colour.y(), colour.z(), 1.0));
    }

    _lineRenderable.update(_wireShader);
}

void PencilTool::renderOverlay()
{
    if (!_cameraMode || _points.size() < 2)
    {
        return;
    }

    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadMatrixd(_cameraProjection);

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadMatrixd(_cameraModelView);

    Vector3 colour = GlobalColourSchemeManager().getColour("drag_selection");

    glDisable(GL_LIGHTING);
    glDisable(GL_TEXTURE_2D);
    glDisable(GL_DEPTH_TEST);
    GlobalRenderingQualitySettings().applyLineSmoothing();

    glColor3f(static_cast<float>(colour.x()), static_cast<float>(colour.y()), static_cast<float>(colour.z()));
    glLineWidth(2.0f);

    glBegin(GL_LINE_STRIP);

    for (const auto& point : _points)
    {
        glVertex3d(point.x(), point.y(), point.z());
    }

    glEnd();

    glLineWidth(1.0f);
    GlobalRenderingQualitySettings().disableSmoothing();
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_LIGHTING);

    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
}

Vector2 PencilTool::getPixelPosition(Event& ev) const
{
    const Vector2& device = ev.getDevicePosition();
    IInteractiveView& view = ev.getInteractiveView();

    return Vector2(device.x() * view.getDeviceWidth() * 0.5, device.y() * view.getDeviceHeight() * 0.5);
}

Vector3 PencilTool::getOrthoPoint(XYMouseToolEvent& xyEvent) const
{
    Vector3 point = xyEvent.getWorldPos();

    int depthAxis = getDepthAxis(xyEvent.getViewType());
    const selection::WorkZone& workZone = GlobalSelectionSystem().getWorkZone();

    point[depthAxis] = float_snapped(
        (workZone.min[depthAxis] + workZone.max[depthAxis]) * 0.5,
        GlobalGrid().getGridSize()
    );

    return point;
}

bool PencilTool::setUpConstructionPlane(CameraMouseToolEvent& camEvent, Vector3& firstPoint)
{
    Vector3 hitPoint;
    Vector3 hitNormal;

    if (findSurfaceUnderCursor(camEvent, hitPoint, hitNormal))
    {
        _constructionPlane = Plane3(hitNormal, hitNormal.dot(hitPoint));
        firstPoint = hitPoint;

        return true;
    }

    const selection::WorkZone& workZone = GlobalSelectionSystem().getWorkZone();
    double planeZ = float_snapped(workZone.bounds.getOrigin().z(), GlobalGrid().getGridSize());

    _constructionPlane = Plane3(Vector3(0, 0, 1), planeZ);

    Ray ray = calculateRayForDevicePoint(camEvent.getView(), camEvent.getDevicePosition());
    double distance = ray.getDistance(_constructionPlane);

    if (distance <= 0 || !std::isfinite(distance))
    {
        return false;
    }

    firstPoint = ray.origin + ray.direction * distance;

    return true;
}

void PencilTool::storeCameraMatrices(CameraMouseToolEvent& camEvent)
{
    _cameraModelView = camEvent.getView().getModelView();
    _cameraProjection = camEvent.getView().getProjection();
}

Vector3 PencilTool::getCameraPoint(CameraMouseToolEvent& camEvent) const
{
    Ray ray = calculateRayForDevicePoint(camEvent.getView(), camEvent.getDevicePosition());
    double distance = ray.getDistance(_constructionPlane);

    if (distance <= 0 || !std::isfinite(distance))
    {
        return _points.back();
    }

    return ray.origin + ray.direction * distance;
}

Ray PencilTool::calculateRayForDevicePoint(camera::ICameraView& camView, const Vector2& devicePoint) const
{
    Matrix4 viewProj = camView.getProjection().getMultipliedBy(camView.getModelView());
    Matrix4 invViewProj = viewProj.getFullInverse();

    Vector4 nearWorld = invViewProj.transform(Vector4(devicePoint.x(), devicePoint.y(), -1.0, 1.0));
    Vector4 farWorld = invViewProj.transform(Vector4(devicePoint.x(), devicePoint.y(), 1.0, 1.0));

    return Ray::createForPoints(nearWorld.getProjected(), farWorld.getProjected());
}

bool PencilTool::findSurfaceUnderCursor(CameraMouseToolEvent& camEvent,
    Vector3& outPoint, Vector3& outNormal) const
{
    SelectionTestPtr selectionTest = camEvent.getView().createSelectionTestForPoint(
        camEvent.getDevicePosition());
    const Matrix4& viewProjection = selectionTest->getVolume().GetViewProjection();

    FaceIntersectionFinder finder(*selectionTest, viewProjection);
    GlobalSceneGraph().root()->traverse(finder);

    const FaceIntersection& intersection = finder.getResult();

    if (!intersection.valid)
    {
        return false;
    }

    outPoint = intersection.point;
    outNormal = intersection.normal;

    return true;
}

void PencilTool::beginStroke(const Vector3& point, const Vector2& pixel, bool cameraMode)
{
    _points.clear();
    _points.push_back(point);
    _lastSamplePixel = pixel;
    _drawing = true;
    _cameraMode = cameraMode;
    _straightLine = false;

    updateRenderables();
}

void PencilTool::addSample(const Vector3& point, const Vector2& pixel)
{
    _straightLine = wxGetKeyState(WXK_CONTROL);

    if (_straightLine)
    {
        _points.resize(1);
        _points.push_back(point);
        _lastSamplePixel = pixel;

        updateRenderables();
        return;
    }

    if ((pixel - _lastSamplePixel).getLength() < SAMPLE_DISTANCE_PIXELS)
    {
        return;
    }

    _points.push_back(point);
    _lastSamplePixel = pixel;

    updateRenderables();
}

void PencilTool::finishStroke()
{
    auto& settings = PencilToolSettings::Instance();

    auto stroke = strokegeometry::processStroke(_points, settings.smooth && !_straightLine, settings.smoothing);

    reset();

    if (stroke.size() >= 3)
    {
        UndoableCommand command("pencilDrawCurve");
        createCurveEntity(stroke);
    }

    GlobalMainFrame().updateAllWindows();
}

void PencilTool::reset()
{
    _points.clear();
    _renderVertices.clear();
    _drawing = false;
    _cameraMode = false;
    _straightLine = false;
    _lineRenderable.clear();
}

void PencilTool::updateRenderables()
{
    _renderVertices.clear();

    for (const auto& point : _points)
    {
        _renderVertices.push_back(Vertex3(point));
    }

    _lineRenderable.queueUpdate();
}

void PencilTool::createCurveEntity(const std::vector<Vector3>& stroke)
{
    auto eclass = GlobalEntityClassManager().findOrInsert(
        getGameKey(GKEY_DEFAULT_CURVE_ENTITY, "func_static"), true);

    auto node = GlobalEntityModule().createEntity(eclass);

    GlobalSceneGraph().root()->addChildNode(node);

    node->getEntity().setKeyValue("origin", string::to_string(getCentroid(stroke)));
    node->getEntity().setKeyValue(getGameKey(GKEY_CURVE_CATMULLROM_KEY, "curve_CatmullRomSpline"),
        serialiseCurve(stroke));

    GlobalSelectionSystem().setSelectedAll(false);
}

}
