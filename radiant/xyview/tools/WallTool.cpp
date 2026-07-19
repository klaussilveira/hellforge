#include "WallTool.h"

#include "i18n.h"
#include "ui/imainframe.h"
#include "igrid.h"
#include "ibrush.h"
#include "iundo.h"
#include "iclipper.h"
#include "iscenegraph.h"
#include "ui/istatusbarmanager.h"
#include "scenelib.h"
#include "math/FloatTools.h"
#include "math/pi.h"
#include "XYMouseToolEvent.h"
#include "WallGeometry.h"
#include "camera/tools/CameraMouseToolEvent.h"
#include "camera/tools/FaceIntersectionFinder.h"
#include "../GlobalXYWnd.h"

#include <algorithm>
#include <deque>
#include <map>
#include <set>

#include <wx/utils.h>

namespace ui
{

namespace
{

using PointKey = std::pair<long long, long long>;

PointKey makeKey(const Vector2& point)
{
    return { std::llround(point.x() * 8.0), std::llround(point.y() * 8.0) };
}

bool pointsEqual(const Vector2& a, const Vector2& b)
{
    return std::abs(a.x() - b.x()) < 0.01 && std::abs(a.y() - b.y()) < 0.01;
}

}

WallToolSettings& WallToolSettings::Instance()
{
    static WallToolSettings _instance;
    return _instance;
}

WallTool::~WallTool()
{
    _mapEventConn.disconnect();
}

const std::string& WallTool::getName()
{
    static std::string name("WallTool");
    return name;
}

const std::string& WallTool::getDisplayName()
{
    static std::string displayName(_("Draw Walls"));
    return displayName;
}

MouseTool::Result WallTool::onMouseDown(Event& ev)
{
    auto& settings = WallToolSettings::Instance();

    if (!settings.active || GlobalClipper().clipMode() ||
        GlobalMapModule().getEditMode() == IMap::EditMode::Merge ||
        GlobalXYWnd().polygonMode())
    {
        return Result::Ignored;
    }

    bool erase = wxGetKeyState(WXK_CONTROL);
    bool chain = wxGetKeyState(WXK_ALT);

    try
    {
        auto& xyEvent = dynamic_cast<XYMouseToolEvent&>(ev);

        if (xyEvent.getViewType() != OrthoOrientation::XY)
        {
            return Result::Ignored;
        }

        Vector3 rawPoint = xyEvent.getWorldPos();
        Vector3 point = rawPoint;
        xyEvent.getView().snapToGrid(point);

        _baseZ = getDefaultBaseZ();

        if (erase)
        {
            _cameraDrag = false;
            startErase(Vector2(rawPoint.x(), rawPoint.y()));
            return Result::Activated;
        }

        if (chain)
        {
            chainWallTo(Vector2(point.x(), point.y()));
            return Result::Finished;
        }

        startDrag(Vector2(point.x(), point.y()), false);

        return Result::Activated;
    }
    catch (std::bad_cast&)
    {
    }

    try
    {
        auto& camEvent = dynamic_cast<CameraMouseToolEvent&>(ev);

        Vector3 hover;

        if (!computeCameraHover(camEvent, hover))
        {
            return Result::Ignored;
        }

        _baseZ = hover.z();
        _constructionPlane = Plane3(0, 0, 1, _baseZ);

        if (erase)
        {
            _cameraDrag = true;
            startErase(Vector2(hover.x(), hover.y()));
            return Result::Activated;
        }

        if (chain)
        {
            chainWallTo(Vector2(hover.x(), hover.y()));
            return Result::Finished;
        }

        startDrag(Vector2(hover.x(), hover.y()), true);

        return Result::Activated;
    }
    catch (std::bad_cast&)
    {
    }

    return Result::Ignored;
}

MouseTool::Result WallTool::onMouseMove(Event& ev)
{
    auto& settings = WallToolSettings::Instance();

    if (!settings.active)
    {
        return Result::Ignored;
    }

    try
    {
        auto& xyEvent = dynamic_cast<XYMouseToolEvent&>(ev);

        if (xyEvent.getViewType() != OrthoOrientation::XY || ((_dragging || _erasing) && _cameraDrag))
        {
            return Result::Ignored;
        }

        Vector3 rawPoint = xyEvent.getWorldPos();
        Vector3 point = rawPoint;
        xyEvent.getView().snapToGrid(point);

        if (_erasing)
        {
            eraseAt(Vector2(rawPoint.x(), rawPoint.y()));
            return Result::Continued;
        }

        if (_dragging)
        {
            updateDrag(Vector2(point.x(), point.y()));
            return Result::Continued;
        }

        double baseZ = getDefaultBaseZ();

        settings.hoverPoint = Vector3(point.x(), point.y(), baseZ);
        settings.hoverValid = true;
        settings.hoverConnected = isConnectedEndpoint(Vector2(point.x(), point.y()), baseZ);
        updateChainPreview(Vector2(point.x(), point.y()), baseZ);
        GlobalMainFrame().updateAllWindows();

        return Result::Ignored;
    }
    catch (std::bad_cast&)
    {
    }

    try
    {
        auto& camEvent = dynamic_cast<CameraMouseToolEvent&>(ev);

        if ((_dragging || _erasing) && !_cameraDrag)
        {
            return Result::Ignored;
        }

        if (_dragging || _erasing)
        {
            Ray ray = calculateRay(camEvent.getView(), ev.getDevicePosition());
            double distance = ray.getDistance(_constructionPlane);

            if (distance > 0 && std::isfinite(distance))
            {
                Vector3 point = ray.origin + ray.direction * distance;

                if (_erasing)
                {
                    eraseAt(Vector2(point.x(), point.y()));
                }
                else
                {
                    double gridSize = GlobalGrid().getGridSize();
                    updateDrag(Vector2(float_snapped(point.x(), gridSize), float_snapped(point.y(), gridSize)));
                }
            }

            return Result::Continued;
        }

        Vector3 hover;

        if (computeCameraHover(camEvent, hover))
        {
            settings.hoverPoint = hover;
            settings.hoverValid = true;
            settings.hoverConnected = isConnectedEndpoint(Vector2(hover.x(), hover.y()), hover.z());
            updateChainPreview(Vector2(hover.x(), hover.y()), hover.z());
            GlobalMainFrame().updateAllWindows();
        }

        return Result::Ignored;
    }
    catch (std::bad_cast&)
    {
    }

    return Result::Ignored;
}

MouseTool::Result WallTool::onMouseUp(Event& ev)
{
    if (!_dragging && !_erasing)
    {
        return Result::Ignored;
    }

    bool applicable = false;

    try
    {
        dynamic_cast<XYMouseToolEvent&>(ev);
        applicable = !_cameraDrag;
    }
    catch (std::bad_cast&)
    {
    }

    try
    {
        dynamic_cast<CameraMouseToolEvent&>(ev);
        applicable = _cameraDrag;
    }
    catch (std::bad_cast&)
    {
    }

    if (!applicable)
    {
        return Result::Ignored;
    }

    if (_erasing)
    {
        if (_erasedAny)
        {
            GlobalUndoSystem().finish("deleteWalls");
        }
        else
        {
            GlobalUndoSystem().cancel();
        }

        resetDragState();

        return Result::Finished;
    }

    if (!_brush || !_hasSegment || !_brush->getParent())
    {
        if (_brush && _brush->getParent())
        {
            scene::removeNodeFromParent(_brush);
        }

        GlobalUndoSystem().cancel();
    }
    else
    {
        commitSegment();
        GlobalUndoSystem().finish("wallSegment");

        _lastPoint = _segmentEnd;
        _lastPointValid = true;
    }

    resetDragState();

    return Result::Finished;
}

MouseTool::Result WallTool::onCancel(IInteractiveView& view)
{
    if (_dragging || _erasing)
    {
        abortDrag();
    }

    return Result::Finished;
}

void WallTool::onMouseCaptureLost(IInteractiveView& view)
{
    if (_dragging || _erasing)
    {
        abortDrag();
    }
}

bool WallTool::alwaysReceivesMoveEvents()
{
    return true;
}

unsigned int WallTool::getPointerMode()
{
    return PointerMode::Capture;
}

unsigned int WallTool::getRefreshMode()
{
    return RefreshMode::Force | RefreshMode::AllViews;
}

void WallTool::startDrag(const Vector2& point, bool cameraDrag)
{
    _anchor = point;
    _segmentEnd = point;
    _dragging = true;
    _cameraDrag = cameraDrag;
    _hasSegment = false;
    _brush.reset();

    GlobalUndoSystem().start();
}

void WallTool::updateDrag(const Vector2& current)
{
    auto& settings = WallToolSettings::Instance();

    Vector2 end = wallgeometry::snapSegmentEnd(_anchor, current, GlobalGrid().getGridSize());

    settings.hoverPoint = Vector3(end.x(), end.y(), _baseZ);
    settings.hoverValid = true;
    settings.hoverConnected = isConnectedEndpoint(end, _baseZ);

    if ((end - _anchor).getLengthSquared() < 1e-9)
    {
        settings.segmentPreviewValid = false;
        return;
    }

    settings.segmentPreviewValid = true;
    settings.segmentPreviewGhost = false;
    settings.segmentPreviewStart = _anchor;
    settings.segmentPreviewEnd = end;
    settings.segmentPreviewBaseZ = _baseZ;

    if (!_brush)
    {
        _brush = GlobalBrushCreator().createBrush();

        if (!_brush)
        {
            return;
        }

        auto worldspawn = GlobalMapModule().findOrInsertWorldspawn();
        scene::addNodeToContainer(_brush, worldspawn);
    }

    if (!_brush->getParent())
    {
        _brush.reset();
        _hasSegment = false;
        return;
    }

    _segmentEnd = end;
    _hasSegment = true;

    wallgeometry::buildWallSegmentFaces(*Node_getIBrush(_brush), _anchor, end,
        _baseZ, settings.wallHeight, settings.wallThickness, settings.wallMaterial);
}

void WallTool::abortDrag()
{
    if (_erasing)
    {
        if (_erasedAny)
        {
            GlobalUndoSystem().finish("deleteWalls");
        }
        else
        {
            GlobalUndoSystem().cancel();
        }

        resetDragState();
        return;
    }

    if (_brush && _brush->getParent())
    {
        scene::removeNodeFromParent(_brush);
    }

    GlobalUndoSystem().cancel();
    resetDragState();
}

void WallTool::resetDragState()
{
    _dragging = false;
    _cameraDrag = false;
    _hasSegment = false;
    _erasing = false;
    _erasedAny = false;
    _brush.reset();

    WallToolSettings::Instance().segmentPreviewValid = false;
}

void WallTool::updateChainPreview(const Vector2& point, double baseZ)
{
    auto& settings = WallToolSettings::Instance();

    if (!_lastPointValid || !wxGetKeyState(WXK_ALT))
    {
        settings.segmentPreviewValid = false;
        return;
    }

    Vector2 end = wallgeometry::snapSegmentEnd(_lastPoint, point, GlobalGrid().getGridSize());

    if ((end - _lastPoint).getLengthSquared() < 1e-9)
    {
        settings.segmentPreviewValid = false;
        return;
    }

    settings.segmentPreviewValid = true;
    settings.segmentPreviewGhost = true;
    settings.segmentPreviewStart = _lastPoint;
    settings.segmentPreviewEnd = end;
    settings.segmentPreviewBaseZ = baseZ;
    settings.hoverConnected = isConnectedEndpoint(end, baseZ);
}

bool WallTool::isConnectedEndpoint(const Vector2& point, double baseZ) const
{
    const auto& settings = WallToolSettings::Instance();

    for (const auto& seg : _segments)
    {
        auto node = seg.node.lock();

        if (!node || !node->getParent()) continue;

        if (std::abs(seg.baseZ - baseZ) > 0.01 || std::abs(seg.height - settings.wallHeight) > 0.01)
        {
            continue;
        }

        if (pointsEqual(seg.a, point) || pointsEqual(seg.b, point))
        {
            return true;
        }
    }

    return false;
}

double WallTool::getDefaultBaseZ() const
{
    return 0.0;
}

bool WallTool::computeCameraHover(CameraMouseToolEvent& camEvent, Vector3& result) const
{
    double gridSize = GlobalGrid().getGridSize();

    Vector3 hitPoint, hitNormal;

    if (findSurfaceUnderCursor(camEvent, hitPoint, hitNormal) && std::abs(hitNormal.z()) > 0.9)
    {
        result = Vector3(float_snapped(hitPoint.x(), gridSize),
            float_snapped(hitPoint.y(), gridSize),
            float_snapped(hitPoint.z(), gridSize));
        return true;
    }

    double baseZ = getDefaultBaseZ();

    Ray ray = calculateRay(camEvent.getView(), camEvent.getDevicePosition());
    double distance = ray.getDistance(Plane3(0, 0, 1, baseZ));

    if (distance <= 0 || !std::isfinite(distance))
    {
        return false;
    }

    Vector3 point = ray.origin + ray.direction * distance;
    result = Vector3(float_snapped(point.x(), gridSize), float_snapped(point.y(), gridSize), baseZ);

    return true;
}

Ray WallTool::calculateRay(camera::ICameraView& camView, const Vector2& devicePoint) const
{
    Matrix4 viewProj = camView.getProjection().getMultipliedBy(camView.getModelView());
    Matrix4 invViewProj = viewProj.getFullInverse();

    Vector4 nearWorld = invViewProj.transform(Vector4(devicePoint.x(), devicePoint.y(), -1.0, 1.0));
    Vector4 farWorld = invViewProj.transform(Vector4(devicePoint.x(), devicePoint.y(), 1.0, 1.0));

    return Ray::createForPoints(nearWorld.getProjected(), farWorld.getProjected());
}

bool WallTool::findSurfaceUnderCursor(CameraMouseToolEvent& camEvent, Vector3& outPoint, Vector3& outNormal) const
{
    SelectionTestPtr selectionTest = camEvent.getView().createSelectionTestForPoint(camEvent.getDevicePosition());
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

void WallTool::ensureMapConnection()
{
    if (!_mapEventConn.connected())
    {
        _mapEventConn = GlobalMapModule().signal_mapEvent().connect(
            sigc::mem_fun(*this, &WallTool::onMapEvent));
    }
}

void WallTool::onMapEvent(IMap::MapEvent ev)
{
    if (ev == IMap::MapUnloading || ev == IMap::MapLoaded)
    {
        _segments.clear();
        _loops.clear();
    }
}

void WallTool::commitSegment()
{
    ensureMapConnection();

    auto& settings = WallToolSettings::Instance();

    WallSegment seg;
    seg.a = _anchor;
    seg.b = _segmentEnd;
    seg.baseZ = _baseZ;
    seg.height = settings.wallHeight;
    seg.thickness = settings.wallThickness;
    seg.material = settings.wallMaterial;
    seg.node = _brush;

    pruneSegments();
    mergeCollinear(seg);
    applyCornerJoints(seg);

    _segments.push_back(seg);

    detectLoopAndFill(seg);
}

void WallTool::pruneSegments()
{
    for (auto it = _segments.begin(); it != _segments.end();)
    {
        auto node = it->node.lock();

        if (!node || !node->getParent())
        {
            it = _segments.erase(it);
        }
        else
        {
            ++it;
        }
    }
}

void WallTool::mergeCollinear(WallSegment& seg)
{
    bool changed = false;

    for (bool merged = true; merged;)
    {
        merged = false;

        Vector2 dir = (seg.b - seg.a).getNormalised();

        for (auto it = _segments.begin(); it != _segments.end(); ++it)
        {
            auto node = it->node.lock();

            if (!node || !node->getParent()) continue;

            if (std::abs(it->baseZ - seg.baseZ) > 0.01 ||
                std::abs(it->height - seg.height) > 0.01 ||
                std::abs(it->thickness - seg.thickness) > 0.01 ||
                it->material != seg.material)
            {
                continue;
            }

            Vector2 otherDir = (it->b - it->a).getNormalised();

            if (std::abs(dir.x() * otherDir.y() - dir.y() * otherDir.x()) > 0.001)
            {
                continue;
            }

            const Vector2 newEnds[2] = { seg.a, seg.b };
            const Vector2 oldEnds[2] = { it->a, it->b };

            Vector2 shared, farNew, farOld;
            bool found = false;

            for (int n = 0; n < 2 && !found; ++n)
            {
                for (int o = 0; o < 2 && !found; ++o)
                {
                    if (pointsEqual(newEnds[n], oldEnds[o]))
                    {
                        shared = newEnds[n];
                        farNew = newEnds[1 - n];
                        farOld = oldEnds[1 - o];
                        found = true;
                    }
                }
            }

            if (!found)
            {
                continue;
            }

            if ((farNew - shared).dot(farOld - shared) > -1e-6)
            {
                continue;
            }

            auto oldCap = pointsEqual(farOld, it->a) ? it->capA : it->capB;
            auto newCap = pointsEqual(farNew, seg.a) ? seg.capA : seg.capB;

            scene::removeNodeFromParent(node);
            seg.a = farOld;
            seg.b = farNew;
            seg.capA = oldCap;
            seg.capB = newCap;
            _segments.erase(it);

            changed = true;
            merged = true;
            break;
        }
    }

    if (changed)
    {
        rebuildSegmentBrush(seg);
    }
}

void WallTool::applyCornerJoints(WallSegment& seg)
{
    bool hadCapA = seg.capA.has_value();
    bool hadCapB = seg.capB.has_value();

    applyCornerJointAt(seg, true);
    applyCornerJointAt(seg, false);

    if (seg.capA.has_value() != hadCapA || seg.capB.has_value() != hadCapB)
    {
        rebuildSegmentBrush(seg);
    }
}

void WallTool::applyCornerJointAt(WallSegment& seg, bool atEndA)
{
    if (atEndA ? seg.capA.has_value() : seg.capB.has_value())
    {
        return;
    }

    const Vector2 corner = atEndA ? seg.a : seg.b;
    Vector2 segAway = ((atEndA ? seg.b : seg.a) - corner).getNormalised();

    for (auto& other : _segments)
    {
        auto node = other.node.lock();

        if (!node || !node->getParent()) continue;

        if (std::abs(other.baseZ - seg.baseZ) > 0.01 || std::abs(other.height - seg.height) > 0.01)
        {
            continue;
        }

        bool otherAtA;

        if (pointsEqual(other.a, corner))
        {
            otherAtA = true;
        }
        else if (pointsEqual(other.b, corner))
        {
            otherAtA = false;
        }
        else
        {
            continue;
        }

        if (otherAtA ? other.capA.has_value() : other.capB.has_value())
        {
            continue;
        }

        Vector2 otherAway = ((otherAtA ? other.b : other.a) - corner).getNormalised();

        auto caps = wallgeometry::computeButtJointCaps(corner, segAway, seg.thickness,
            otherAway, other.thickness);

        if (!caps)
        {
            continue;
        }

        if (atEndA)
        {
            seg.capA = caps->segmentCap;
        }
        else
        {
            seg.capB = caps->segmentCap;
        }

        if (otherAtA)
        {
            other.capA = caps->otherCap;
        }
        else
        {
            other.capB = caps->otherCap;
        }

        rebuildSegmentBrush(other);
        return;
    }
}

void WallTool::rebuildSegmentBrush(const WallSegment& seg)
{
    auto node = seg.node.lock();

    if (!node || !node->getParent())
    {
        return;
    }

    wallgeometry::buildWallSegmentFaces(*Node_getIBrush(node), seg.a, seg.b,
        seg.baseZ, seg.height, seg.thickness, seg.material, seg.capA, seg.capB);
}

void WallTool::chainWallTo(const Vector2& point)
{
    auto& settings = WallToolSettings::Instance();

    if (!_lastPointValid)
    {
        _lastPoint = point;
        _lastPointValid = true;
        settings.hoverPoint = Vector3(point.x(), point.y(), _baseZ);
        settings.hoverValid = true;
        return;
    }

    Vector2 end = wallgeometry::snapSegmentEnd(_lastPoint, point, GlobalGrid().getGridSize());

    if ((end - _lastPoint).getLengthSquared() < 1e-9)
    {
        return;
    }

    GlobalUndoSystem().start();

    _brush = GlobalBrushCreator().createBrush();

    if (!_brush)
    {
        GlobalUndoSystem().cancel();
        return;
    }

    auto worldspawn = GlobalMapModule().findOrInsertWorldspawn();
    scene::addNodeToContainer(_brush, worldspawn);

    _anchor = _lastPoint;
    _segmentEnd = end;
    _hasSegment = true;

    wallgeometry::buildWallSegmentFaces(*Node_getIBrush(_brush), _anchor, end,
        _baseZ, settings.wallHeight, settings.wallThickness, settings.wallMaterial);

    commitSegment();
    GlobalUndoSystem().finish("wallSegment");

    _lastPoint = end;
    settings.hoverPoint = Vector3(end.x(), end.y(), _baseZ);
    settings.hoverValid = true;

    resetDragState();
}

void WallTool::startErase(const Vector2& point)
{
    _erasing = true;
    _erasedAny = false;

    GlobalUndoSystem().start();
    eraseAt(point);
}

void WallTool::eraseAt(const Vector2& point)
{
    double tolerance = std::max(2.0, GlobalGrid().getGridSize() * 0.25);

    for (auto it = _segments.begin(); it != _segments.end();)
    {
        auto node = it->node.lock();

        if (!node || !node->getParent())
        {
            it = _segments.erase(it);
            continue;
        }

        if (wallgeometry::distanceToSegment(point, it->a, it->b) <= it->thickness * 0.5 + tolerance)
        {
            scene::removeNodeFromParent(node);
            it = _segments.erase(it);
            _erasedAny = true;
            continue;
        }

        ++it;
    }
}

void WallTool::detectLoopAndFill(const WallSegment& seg)
{
    std::map<PointKey, std::vector<std::pair<PointKey, Vector2>>> adjacency;

    for (std::size_t i = 0; i + 1 < _segments.size(); ++i)
    {
        const auto& other = _segments[i];
        auto node = other.node.lock();

        if (!node || !node->getParent()) continue;

        if (std::abs(other.baseZ - seg.baseZ) > 0.01 || std::abs(other.height - seg.height) > 0.01)
        {
            continue;
        }

        auto keyA = makeKey(other.a);
        auto keyB = makeKey(other.b);

        adjacency[keyA].push_back({ keyB, other.b });
        adjacency[keyB].push_back({ keyA, other.a });
    }

    auto start = makeKey(seg.a);
    auto goal = makeKey(seg.b);

    std::map<PointKey, std::pair<PointKey, Vector2>> parent;
    std::set<PointKey> visited;
    std::deque<PointKey> queue;

    visited.insert(start);
    queue.push_back(start);

    bool foundGoal = false;

    while (!queue.empty() && !foundGoal)
    {
        auto current = queue.front();
        queue.pop_front();

        for (const auto& [nextKey, nextPoint] : adjacency[current])
        {
            if (visited.count(nextKey) > 0) continue;

            visited.insert(nextKey);
            parent[nextKey] = { current, nextPoint };

            if (nextKey == goal)
            {
                foundGoal = true;
                break;
            }

            queue.push_back(nextKey);
        }
    }

    if (!foundGoal)
    {
        return;
    }

    std::vector<Vector2> reversedPath;

    for (auto key = goal; key != start;)
    {
        const auto& [prevKey, point] = parent[key];
        reversedPath.push_back(point);
        key = prevKey;
    }

    std::vector<Vector2> polygon;
    polygon.push_back(seg.a);

    for (auto it = reversedPath.rbegin(); it != reversedPath.rend(); ++it)
    {
        polygon.push_back(*it);
    }

    polygon = wallgeometry::simplifyCollinear(polygon);

    if (polygon.size() < 3)
    {
        return;
    }

    if (wallgeometry::signedArea(polygon) < 0)
    {
        std::reverse(polygon.begin(), polygon.end());
    }

    std::vector<PointKey> keys;

    for (const auto& point : polygon)
    {
        keys.push_back(makeKey(point));
    }

    std::sort(keys.begin(), keys.end());

    std::string loopKey;

    for (const auto& key : keys)
    {
        loopKey += std::to_string(key.first) + ":" + std::to_string(key.second) + ";";
    }

    loopKey += "z" + std::to_string(std::llround(seg.baseZ * 8.0));

    for (auto it = _loops.begin(); it != _loops.end();)
    {
        auto node = it->floorNode.lock();

        if (!node || !node->getParent())
        {
            it = _loops.erase(it);
        }
        else
        {
            ++it;
        }
    }

    for (const auto& loop : _loops)
    {
        if (loop.key == loopKey)
        {
            return;
        }
    }

    _loops.push_back({ loopKey, scene::INodeWeakPtr() });

    generateRoom(polygon, seg);
}

void WallTool::generateRoom(const std::vector<Vector2>& polygon, const WallSegment& seg)
{
    auto& settings = WallToolSettings::Instance();

    std::vector<std::vector<Vector2>> pieces;

    if (wallgeometry::isConvex(polygon))
    {
        pieces.push_back(polygon);
    }
    else
    {
        pieces = wallgeometry::decomposeIntoConvex(polygon);
    }

    if (pieces.empty())
    {
        GlobalStatusBarManager().setText("Commands",
            _("Wall Tool: could not generate a floor for this room"));
        return;
    }

    auto worldspawn = GlobalMapModule().findOrInsertWorldspawn();

    scene::INodePtr firstFloor;

    for (const auto& piece : pieces)
    {
        auto node = GlobalBrushCreator().createBrush();
        scene::addNodeToContainer(node, worldspawn);

        wallgeometry::buildPrismFaces(*Node_getIBrush(node), piece,
            seg.baseZ - settings.floorThickness, seg.baseZ, settings.floorMaterial);

        if (!firstFloor)
        {
            firstFloor = node;
        }
    }

    _loops.back().floorNode = firstFloor;

    double topZ = seg.baseZ + seg.height;
    auto roofType = settings.roofType;

    if (roofType != WallRoofType::Flat && !wallgeometry::isAxisAlignedRectangle(polygon))
    {
        GlobalStatusBarManager().setText("Commands",
            _("Wall Tool: room is not rectangular, using a flat roof"));
        roofType = WallRoofType::Flat;
    }

    if (roofType == WallRoofType::Flat)
    {
        for (const auto& piece : pieces)
        {
            auto node = GlobalBrushCreator().createBrush();
            scene::addNodeToContainer(node, worldspawn);

            wallgeometry::buildPrismFaces(*Node_getIBrush(node), piece,
                topZ, topZ + settings.floorThickness, settings.roofMaterial);
        }

        return;
    }

    Vector2 mins = polygon[0];
    Vector2 maxs = polygon[0];

    for (const auto& point : polygon)
    {
        mins.x() = std::min(mins.x(), point.x());
        mins.y() = std::min(mins.y(), point.y());
        maxs.x() = std::max(maxs.x(), point.x());
        maxs.y() = std::max(maxs.y(), point.y());
    }

    double span = std::min(maxs.x() - mins.x(), maxs.y() - mins.y());
    double pitch = std::tan(settings.roofPitch * math::PI / 180.0);

    if (roofType == WallRoofType::Shed)
    {
        auto node = GlobalBrushCreator().createBrush();
        scene::addNodeToContainer(node, worldspawn);

        wallgeometry::buildShedRoofFaces(*Node_getIBrush(node), mins, maxs,
            topZ, pitch * span, settings.roofMaterial);
    }
    else
    {
        for (bool firstHalf : { true, false })
        {
            auto node = GlobalBrushCreator().createBrush();
            scene::addNodeToContainer(node, worldspawn);

            wallgeometry::buildGabledRoofWedgeFaces(*Node_getIBrush(node), mins, maxs,
                topZ, pitch * span * 0.5, firstHalf, settings.roofMaterial);
        }
    }
}

}
