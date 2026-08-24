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
#include "polygon/Polygon2D.h"
#include "XYMouseToolEvent.h"
#include "WallGeometry.h"
#include "camera/tools/CameraMouseToolEvent.h"
#include "camera/tools/FaceIntersectionFinder.h"
#include "../GlobalXYWnd.h"

#include <algorithm>
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

std::string quantisedKey(double value)
{
    return std::to_string(std::llround(value * 8.0));
}

std::string levelKey(const WallSegment& seg)
{
    return quantisedKey(seg.baseZ) + ":" + quantisedKey(seg.height);
}

std::string wallGroupKey(const WallSegment& seg)
{
    return levelKey(seg) + ":" + quantisedKey(seg.thickness) + ":" + seg.material;
}

std::vector<polygon::Ring> regionRings(const polygon::Region& region)
{
    std::vector<polygon::Ring> rings;

    rings.push_back(region.outer);
    rings.insert(rings.end(), region.holes.begin(), region.holes.end());

    return rings;
}

std::string roomKey(const std::string& level, const polygon::Ring& outline)
{
    std::vector<PointKey> keys;

    for (const Vector2& point : outline)
    {
        keys.push_back(makeKey(point));
    }

    std::size_t start = 0;

    for (std::size_t i = 1; i < keys.size(); ++i)
    {
        if (keys[i] < keys[start])
        {
            start = i;
        }
    }

    std::string key = level;

    for (std::size_t i = 0; i < keys.size(); ++i)
    {
        const PointKey& point = keys[(start + i) % keys.size()];
        key += "|" + std::to_string(point.first) + "," + std::to_string(point.second);
    }

    return key;
}

}

WallToolSettings& WallToolSettings::Instance()
{
    static WallToolSettings _instance;
    return _instance;
}

WallTool::WallTool() :
    _undoable(_state, [this](const WallToolState& state) { _state = state; })
{}

WallTool::~WallTool()
{
    releaseUndoable();
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

    bool commit = _brush && _hasSegment && _brush->getParent();

    if (_brush && _brush->getParent())
    {
        scene::removeNodeFromParent(_brush);
    }

    _brush.reset();

    if (commit)
    {
        GlobalUndoSystem().start();
        saveStateForUndo();

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

    for (const auto& seg : _state.segments)
    {
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

    if (!_undoable.isConnected())
    {
        _undoable.connectUndoSystem(GlobalUndoSystem());
    }
}

void WallTool::releaseUndoable()
{
    if (_undoable.isConnected())
    {
        try
        {
            _undoable.disconnectUndoSystem(GlobalUndoSystem());
        }
        catch (const std::runtime_error&)
        {
        }
    }
}

void WallTool::saveStateForUndo()
{
    ensureMapConnection();
    _undoable.save();
}

void WallTool::onMapEvent(IMap::MapEvent ev)
{
    if (ev == IMap::MapUnloading || ev == IMap::MapLoaded)
    {
        releaseUndoable();

        _state.segments.clear();
        _state.walls.clear();
        _state.rooms.clear();
    }
}

void WallTool::commitSegment()
{
    auto& settings = WallToolSettings::Instance();

    WallSegment seg;
    seg.a = _anchor;
    seg.b = _segmentEnd;
    seg.baseZ = _baseZ;
    seg.height = settings.wallHeight;
    seg.thickness = settings.wallThickness;
    seg.material = settings.wallMaterial;

    _state.segments.push_back(seg);

    rebuildGeometry({ levelKey(seg) });
}

void WallTool::destroyGeometry(WallToolGeometry& geometry)
{
    for (const scene::INodeWeakPtr& weak : geometry.nodes)
    {
        auto node = weak.lock();

        if (node && node->getParent())
        {
            scene::removeNodeFromParent(node);
        }
    }

    geometry.nodes.clear();
}

void WallTool::rebuildGeometry(const std::set<std::string>& touchedLevels)
{
    if (touchedLevels.empty())
    {
        return;
    }

    std::map<std::string, LevelWalls> levels = collectLevels(touchedLevels);

    rebuildWalls(levels);
    rebuildRooms(levels);
}

std::map<std::string, LevelWalls> WallTool::collectLevels(
    const std::set<std::string>& touchedLevels) const
{
    std::map<std::string, LevelWalls> levels;

    for (const WallSegment& seg : _state.segments)
    {
        std::string key = levelKey(seg);

        if (touchedLevels.count(key) == 0)
        {
            continue;
        }

        LevelWalls& level = levels[key];
        level.baseZ = seg.baseZ;
        level.height = seg.height;
        level.lines.push_back({ seg.a, seg.b, seg.thickness });
    }

    for (const std::string& key : touchedLevels)
    {
        levels[key].mouths = wallgeometry::corridorMouths(levels[key].lines);
    }

    return levels;
}

void WallTool::rebuildWalls(const std::map<std::string, LevelWalls>& levels)
{
    for (auto it = _state.walls.begin(); it != _state.walls.end();)
    {
        if (levels.count(it->level) > 0)
        {
            destroyGeometry(*it);
            it = _state.walls.erase(it);
        }
        else
        {
            ++it;
        }
    }

    std::map<std::string, std::vector<const WallSegment*>> groups;

    for (const WallSegment& seg : _state.segments)
    {
        if (levels.count(levelKey(seg)) > 0)
        {
            groups[wallGroupKey(seg)].push_back(&seg);
        }
    }

    for (const auto& [key, group] : groups)
    {
        WallToolGeometry geometry;
        geometry.key = key;
        geometry.level = levelKey(*group.front());

        createWallBrushes(group, levels.at(geometry.level), geometry);

        if (!geometry.nodes.empty())
        {
            _state.walls.push_back(geometry);
        }
    }
}

void WallTool::createWallBrushes(const std::vector<const WallSegment*>& group,
    const LevelWalls& level, WallToolGeometry& geometry)
{
    if (group.empty())
    {
        return;
    }

    const WallSegment& first = *group.front();

    std::vector<wallgeometry::WallLine> lines;

    for (const WallSegment* seg : group)
    {
        lines.push_back({ seg->a, seg->b, seg->thickness });
    }

    std::vector<polygon::Ring> footprint = polygon::thicken(
        wallgeometry::trimFreeEnds(lines, level.lines), first.thickness);

    footprint = polygon::difference(footprint, level.mouths);

    std::vector<polygon::Ring> pieces = polygon::convexPieces(footprint, polygon::EPSILON);

    if (pieces.empty())
    {
        return;
    }

    auto worldspawn = GlobalMapModule().findOrInsertWorldspawn();

    for (const polygon::Ring& piece : pieces)
    {
        auto node = GlobalBrushCreator().createBrush();

        if (!node)
        {
            continue;
        }

        wallgeometry::buildPrismFaces(*Node_getIBrush(node), piece,
            first.baseZ, first.baseZ + first.height, first.material);

        scene::addNodeToContainer(node, worldspawn);

        geometry.nodes.push_back(node);
    }
}

void WallTool::rebuildRooms(const std::map<std::string, LevelWalls>& levels)
{
    struct Candidate
    {
        std::string level;
        polygon::Region region;
    };

    std::map<std::string, Candidate> wanted;

    for (const auto& [key, level] : levels)
    {
        for (const polygon::Region& region :
             wallgeometry::walkableRegions(level.lines, level.mouths))
        {
            wanted[roomKey(key, region.outer)] = { key, region };
        }
    }

    for (auto it = _state.rooms.begin(); it != _state.rooms.end();)
    {
        if (levels.count(it->level) == 0)
        {
            ++it;
            continue;
        }

        if (wanted.count(it->key) > 0)
        {
            wanted.erase(it->key);
            ++it;
        }
        else
        {
            destroyGeometry(*it);
            it = _state.rooms.erase(it);
        }
    }

    for (const auto& [key, candidate] : wanted)
    {
        const LevelWalls& level = levels.at(candidate.level);

        WallToolGeometry geometry;
        geometry.key = key;
        geometry.level = candidate.level;

        createRoomBrushes(candidate.region, level.baseZ, level.height, geometry);

        if (!geometry.nodes.empty())
        {
            _state.rooms.push_back(geometry);
        }
    }
}

void WallTool::createRoomBrushes(const polygon::Region& region, double baseZ, double height,
    WallToolGeometry& geometry)
{
    auto& settings = WallToolSettings::Instance();

    std::vector<polygon::Ring> rings = regionRings(region);
    std::vector<polygon::Ring> pieces = polygon::convexPieces(rings, polygon::EPSILON);

    if (pieces.empty())
    {
        GlobalStatusBarManager().setText("Commands",
            _("Wall Tool: could not generate a floor for this room"));
        return;
    }

    auto worldspawn = GlobalMapModule().findOrInsertWorldspawn();

    auto addBrush = [&](const polygon::Ring& piece, double bottom, double top,
        const std::string& material)
    {
        auto node = GlobalBrushCreator().createBrush();

        if (!node)
        {
            return;
        }

        wallgeometry::buildPrismFaces(*Node_getIBrush(node), piece, bottom, top, material);
        scene::addNodeToContainer(node, worldspawn);
        geometry.nodes.push_back(node);
    };

    for (const polygon::Ring& piece : pieces)
    {
        addBrush(piece, baseZ - settings.floorThickness, baseZ, settings.floorMaterial);
    }

    double topZ = baseZ + height;

    if (settings.roofType == WallRoofType::Flat)
    {
        for (const polygon::Ring& piece : pieces)
        {
            addBrush(piece, topZ, topZ + settings.floorThickness, settings.roofMaterial);
        }

        return;
    }

    Vector2 mins = region.outer[0];
    Vector2 maxs = region.outer[0];

    for (const Vector2& point : region.outer)
    {
        mins.x() = std::min(mins.x(), point.x());
        mins.y() = std::min(mins.y(), point.y());
        maxs.x() = std::max(maxs.x(), point.x());
        maxs.y() = std::max(maxs.y(), point.y());
    }

    double spanX = maxs.x() - mins.x();
    double spanY = maxs.y() - mins.y();
    double span = std::min(spanX, spanY);
    double pitch = std::tan(settings.roofPitch * math::PI / 180.0);

    int axis = spanY <= spanX ? 1 : 0;
    double low = axis == 1 ? mins.y() : mins.x();
    double high = axis == 1 ? maxs.y() : maxs.x();

    auto slopePlane = [&](double eave, double ridge, double rise)
    {
        double run = ridge - eave;
        double length = std::sqrt(rise * rise + run * run);

        if (length < 1e-6)
        {
            return Plane3(0, 0, 1, topZ);
        }

        double n = -rise / length;
        double nz = std::abs(run) / length;

        if (run < 0)
        {
            n = -n;
        }

        return axis == 1 ? Plane3(0, n, nz, n * eave + nz * topZ)
                         : Plane3(n, 0, nz, n * eave + nz * topZ);
    };

    auto addSloped = [&](const std::vector<polygon::Ring>& footprint, const Plane3& top)
    {
        for (const polygon::Ring& piece : polygon::convexPieces(footprint, polygon::EPSILON))
        {
            auto node = GlobalBrushCreator().createBrush();

            if (!node)
            {
                continue;
            }

            wallgeometry::buildSlopedPrismFaces(*Node_getIBrush(node), piece, topZ, top,
                settings.roofMaterial);
            scene::addNodeToContainer(node, worldspawn);
            geometry.nodes.push_back(node);
        }
    };

    if (settings.roofType == WallRoofType::Shed)
    {
        addSloped(rings, slopePlane(low, high, pitch * span));

        return;
    }

    double middle = (low + high) * 0.5;
    double rise = pitch * span * 0.5;

    polygon::Ring lowHalf, highHalf;

    if (axis == 1)
    {
        lowHalf = { Vector2(mins.x() - 1, mins.y() - 1), Vector2(maxs.x() + 1, mins.y() - 1),
                    Vector2(maxs.x() + 1, middle), Vector2(mins.x() - 1, middle) };
        highHalf = { Vector2(mins.x() - 1, middle), Vector2(maxs.x() + 1, middle),
                     Vector2(maxs.x() + 1, maxs.y() + 1), Vector2(mins.x() - 1, maxs.y() + 1) };
    }
    else
    {
        lowHalf = { Vector2(mins.x() - 1, mins.y() - 1), Vector2(middle, mins.y() - 1),
                    Vector2(middle, maxs.y() + 1), Vector2(mins.x() - 1, maxs.y() + 1) };
        highHalf = { Vector2(middle, mins.y() - 1), Vector2(maxs.x() + 1, mins.y() - 1),
                     Vector2(maxs.x() + 1, maxs.y() + 1), Vector2(middle, maxs.y() + 1) };
    }

    addSloped(polygon::intersect(rings, { lowHalf }), slopePlane(low, middle, rise));
    addSloped(polygon::intersect(rings, { highHalf }), slopePlane(high, middle, rise));
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
    saveStateForUndo();

    _anchor = _lastPoint;
    _segmentEnd = end;
    _hasSegment = true;

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
    saveStateForUndo();
    eraseAt(point);
}

void WallTool::eraseAt(const Vector2& point)
{
    double tolerance = std::max(2.0, GlobalGrid().getGridSize() * 0.25);

    std::set<std::string> touchedLevels;

    for (auto it = _state.segments.begin(); it != _state.segments.end();)
    {
        if (wallgeometry::distanceToSegment(point, it->a, it->b) <= it->thickness * 0.5 + tolerance)
        {
            touchedLevels.insert(levelKey(*it));
            it = _state.segments.erase(it);
            _erasedAny = true;
            continue;
        }

        ++it;
    }

    rebuildGeometry(touchedLevels);
}

}
