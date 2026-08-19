#pragma once

#include "imousetool.h"
#include "icameraview.h"
#include "math/Vector2.h"
#include "math/Vector3.h"
#include "math/Plane3.h"
#include "math/Matrix4.h"
#include "math/Ray.h"
#include "render.h"
#include "render/RenderableVertexArray.h"

#include <sigc++/signal.h>
#include <string>
#include <vector>

namespace ui
{

class XYMouseToolEvent;
class CameraMouseToolEvent;

struct PencilToolSettings
{
    bool active = false;
    bool smooth = true;
    double smoothing = 8.0;

    sigc::signal<void> signal_settingsChanged;

    static PencilToolSettings& Instance();
};

class PencilTool :
    public MouseTool
{
private:
    std::vector<Vector3> _points;
    bool _drawing;
    bool _cameraMode;
    bool _straightLine;
    Vector2 _lastSamplePixel;

    Plane3 _constructionPlane;
    Matrix4 _cameraModelView;
    Matrix4 _cameraProjection;

    std::vector<Vertex3> _renderVertices;
    render::RenderableLine _lineRenderable;
    ShaderPtr _wireShader;

public:
    PencilTool();

    const std::string& getName() override;
    const std::string& getDisplayName() override;

    Result onMouseDown(Event& ev) override;
    Result onMouseMove(Event& ev) override;
    Result onMouseUp(Event& ev) override;

    Result onCancel(IInteractiveView& view) override;
    void onMouseCaptureLost(IInteractiveView& view) override;

    unsigned int getPointerMode() override;
    unsigned int getRefreshMode() override;

    void render(RenderSystem& renderSystem, IRenderableCollector& collector, const VolumeTest& volume) override;
    void renderOverlay() override;

private:
    Vector2 getPixelPosition(Event& ev) const;
    Vector3 getOrthoPoint(XYMouseToolEvent& xyEvent) const;
    bool setUpConstructionPlane(CameraMouseToolEvent& camEvent, Vector3& firstPoint);
    void storeCameraMatrices(CameraMouseToolEvent& camEvent);
    Vector3 getCameraPoint(CameraMouseToolEvent& camEvent) const;
    Ray calculateRayForDevicePoint(camera::ICameraView& camView, const Vector2& devicePoint) const;
    bool findSurfaceUnderCursor(CameraMouseToolEvent& camEvent, Vector3& outPoint, Vector3& outNormal) const;

    void beginStroke(const Vector3& point, const Vector2& pixel, bool cameraMode);
    void addSample(const Vector3& point, const Vector2& pixel);
    void finishStroke();
    void reset();
    void updateRenderables();

    void createCurveEntity(const std::vector<Vector3>& stroke);
};

}
