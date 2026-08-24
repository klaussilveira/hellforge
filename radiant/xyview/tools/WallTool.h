#pragma once

#include "imousetool.h"
#include "inode.h"
#include "imap.h"
#include "icameraview.h"
#include "math/Vector2.h"
#include "math/Vector3.h"
#include "math/Plane3.h"
#include "math/Ray.h"
#include "polygon/Polygon2D.h"
#include "ObservedUndoable.h"
#include "WallGeometry.h"
#include <map>
#include <set>
#include <sigc++/connection.h>
#include <string>
#include <vector>

namespace ui
{

class XYMouseToolEvent;
class CameraMouseToolEvent;

enum class WallRoofType
{
    Flat = 0,
    Shed = 1,
    Gabled = 2
};

struct WallToolSettings
{
    double wallHeight = 128.0;
    double wallThickness = 8.0;
    double floorThickness = 8.0;
    double roofPitch = 30.0;
    WallRoofType roofType = WallRoofType::Flat;
    std::string wallMaterial = "_default";
    std::string floorMaterial = "_default";
    std::string roofMaterial = "_default";

    bool active = false;
    bool hoverValid = false;
    bool hoverConnected = false;
    Vector3 hoverPoint;

    bool segmentPreviewValid = false;
    bool segmentPreviewGhost = false;
    Vector2 segmentPreviewStart;
    Vector2 segmentPreviewEnd;
    double segmentPreviewBaseZ = 0;

    static WallToolSettings& Instance();
};

struct WallSegment
{
    Vector2 a;
    Vector2 b;
    double baseZ;
    double height;
    double thickness;
    std::string material;
};

struct WallToolGeometry
{
    std::string key;
    std::string level;
    std::vector<scene::INodeWeakPtr> nodes;
};

struct LevelWalls
{
    double baseZ = 0;
    double height = 0;
    std::vector<wallgeometry::WallLine> lines;
    std::vector<polygon::Ring> mouths;
};

struct WallToolState
{
    std::vector<WallSegment> segments;
    std::vector<WallToolGeometry> walls;
    std::vector<WallToolGeometry> rooms;
};

class WallTool :
    public MouseTool
{
private:
    bool _dragging = false;
    bool _cameraDrag = false;
    bool _hasSegment = false;
    bool _erasing = false;
    bool _erasedAny = false;
    Vector2 _anchor;
    Vector2 _segmentEnd;
    double _baseZ = 0;
    scene::INodePtr _brush;
    Plane3 _constructionPlane;

    Vector2 _lastPoint;
    bool _lastPointValid = false;

    WallToolState _state;
    undo::ObservedUndoable<WallToolState> _undoable;
    sigc::connection _mapEventConn;

public:
    WallTool();
    ~WallTool();

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

private:
    void startDrag(const Vector2& point, bool cameraDrag);
    void updateDrag(const Vector2& current);
    void abortDrag();
    void resetDragState();

    double getDefaultBaseZ() const;
    bool computeCameraHover(CameraMouseToolEvent& camEvent, Vector3& result) const;
    Ray calculateRay(camera::ICameraView& camView, const Vector2& devicePoint) const;
    bool findSurfaceUnderCursor(CameraMouseToolEvent& camEvent, Vector3& outPoint, Vector3& outNormal) const;

    void ensureMapConnection();
    void onMapEvent(IMap::MapEvent ev);
    void saveStateForUndo();
    void releaseUndoable();

    void commitSegment();
    void rebuildGeometry(const std::set<std::string>& touchedLevels);
    std::map<std::string, LevelWalls> collectLevels(const std::set<std::string>& touchedLevels) const;
    void rebuildWalls(const std::map<std::string, LevelWalls>& levels);
    void rebuildRooms(const std::map<std::string, LevelWalls>& levels);
    void createWallBrushes(const std::vector<const WallSegment*>& group,
        const LevelWalls& level, WallToolGeometry& geometry);
    void createRoomBrushes(const polygon::Region& region, double baseZ, double height,
        WallToolGeometry& geometry);
    static void destroyGeometry(WallToolGeometry& geometry);

    void chainWallTo(const Vector2& point);
    void startErase(const Vector2& point);
    void eraseAt(const Vector2& point);
    void updateChainPreview(const Vector2& point, double baseZ);
    bool isConnectedEndpoint(const Vector2& point, double baseZ) const;
};

}
