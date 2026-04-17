#include "VertexPaintTool.h"

#include "i18n.h"
#include "imodel.h"
#include "iscenegraph.h"
#include "itraceable.h"
#include "ivertexpaintable.h"
#include "math/Ray.h"
#include "math/Matrix4.h"
#include "math/Vector4.h"
#include "os/path.h"
#include "string/case_conv.h"

#include <algorithm>
#include <limits>

namespace ui
{

VertexPaintSettings& VertexPaintSettings::Instance()
{
    static VertexPaintSettings instance;
    return instance;
}

const std::string& VertexPaintTool::getName()
{
    static std::string name("VertexPaintTool");
    return name;
}

const std::string& VertexPaintTool::getDisplayName()
{
    static std::string displayName(_("Vertex Paint"));
    return displayName;
}

namespace
{
    bool buildRayFromEvent(MouseTool::Event& ev, Ray& outRay)
    {
        const VolumeTest& vol = ev.getInteractiveView().getVolumeTest();
        const Vector2& p = ev.getDevicePosition();

        Matrix4 invVP = vol.GetViewProjection().getFullInverse();

        Vector4 near4 = invVP.transform(Vector4(p.x(), p.y(), -1, 1));
        Vector4 far4  = invVP.transform(Vector4(p.x(), p.y(),  1, 1));

        if (near4.w() == 0 || far4.w() == 0) return false;

        Vector3 nearP(near4.x() / near4.w(), near4.y() / near4.w(), near4.z() / near4.w());
        Vector3 farP (far4.x()  / far4.w(),  far4.y()  / far4.w(),  far4.z()  / far4.w());

        outRay = Ray::createForPoints(nearP, farP);
        return true;
    }

    bool raycastTarget(const Ray& ray, const scene::INodePtr& node, Vector3& point)
    {
        if (!node) return false;
        auto traceable = std::dynamic_pointer_cast<ITraceable>(node);
        if (!traceable) return false;
        return traceable->getIntersection(ray, point);
    }

    bool isAsePaintable(const scene::INodePtr& node)
    {
        auto* paintable = Node_getVertexPaintable(node);
        if (!paintable) return false;
        return string::to_lower_copy(os::getExtension(paintable->getPaintableModelFilename())) == "ase";
    }

    class AsePaintablePicker : public scene::NodeVisitor
    {
    public:
        Ray ray;
        scene::INodePtr bestNode;
        Vector3 bestPoint;
        double bestDistSq = std::numeric_limits<double>::infinity();

        bool pre(const scene::INodePtr& node) override
        {
            if (!isAsePaintable(node)) return true;

            Vector3 point;
            if (!raycastTarget(ray, node, point)) return true;

            double d = (point - ray.origin).getLengthSquared();
            if (d < bestDistSq)
            {
                bestDistSq = d;
                bestNode = node;
                bestPoint = point;
            }
            return true;
        }
    };

    scene::INodePtr pickPaintableUnderRay(const Ray& ray, Vector3& outPoint)
    {
        auto root = GlobalSceneGraph().root();
        if (!root) return {};

        AsePaintablePicker picker;
        picker.ray = ray;
        root->traverse(picker);

        outPoint = picker.bestPoint;
        return picker.bestNode;
    }

    class PaintPropagateWalker : public scene::NodeVisitor
    {
    public:
        std::string modelPath;
        Vector3 localCenter;
        VertexPaintChannel channel = VertexPaintChannel::Red;
        const VertexPaintSettings* settings = nullptr;
        scene::INodePtr skipNode;

        bool pre(const scene::INodePtr& node) override
        {
            if (node == skipNode) return true;

            auto* paintable = Node_getVertexPaintable(node);
            if (!paintable) return true;

            if (paintable->getPaintableModelPath() != modelPath) return true;

            bool touched = false;
            std::size_t count = paintable->getPaintableSurfaceCount();
            for (std::size_t i = 0; i < count; ++i)
            {
                if (auto* surf = paintable->getPaintableSurface(i))
                {
                    if (vertexPaint::applyPaint(*surf, localCenter, channel, *settings))
                    {
                        touched = true;
                    }
                }
            }

            if (touched)
            {
                paintable->queueRenderableUpdate();
            }

            return true;
        }
    };
}


MouseTool::Result VertexPaintTool::onMouseDown(Event& ev)
{
    auto& settings = VertexPaintSettings::Instance();
    if (!settings.panelActive) return Result::Ignored;

    Ray ray;
    if (!buildRayFromEvent(ev, ray)) return Result::Ignored;

    Vector3 point;
    scene::INodePtr hit = pickPaintableUnderRay(ray, point);
    if (!hit) return Result::Ignored;

    _strokeTarget = hit;
    settings.target = hit;
    settings.hoverValid = true;
    settings.hoverPoint = point;

    _stroking = true;

    applyStrokeAt(ev);

    return Result::Activated;
}

MouseTool::Result VertexPaintTool::onMouseMove(Event& ev)
{
    if (_stroking)
    {
        applyStrokeAt(ev);
        return Result::Continued;
    }

    auto& settings = VertexPaintSettings::Instance();

    bool prevValid = settings.hoverValid;
    Vector3 prevPoint = settings.hoverPoint;

    settings.hoverValid = false;

    if (settings.panelActive)
    {
        Ray ray;
        if (buildRayFromEvent(ev, ray))
        {
            Vector3 point;
            scene::INodePtr hit = pickPaintableUnderRay(ray, point);
            if (hit)
            {
                settings.hoverValid = true;
                settings.hoverPoint = point;
                settings.target = hit;
            }
        }
    }

    if (settings.hoverValid != prevValid ||
        (settings.hoverValid && settings.hoverPoint != prevPoint))
    {
        ev.getInteractiveView().queueDraw();
    }

    return Result::Ignored;
}

MouseTool::Result VertexPaintTool::onMouseUp(Event& ev)
{
    if (!_stroking) return Result::Ignored;

    _stroking = false;
    _strokeTarget.reset();
    ev.getInteractiveView().queueDraw();
    return Result::Finished;
}

MouseTool::Result VertexPaintTool::onCancel(IInteractiveView&)
{
    _stroking = false;
    _strokeTarget.reset();
    return Result::Finished;
}

void VertexPaintTool::onMouseCaptureLost(IInteractiveView& view)
{
    onCancel(view);
}

bool VertexPaintTool::applyStrokeAt(Event& ev)
{
    const auto& settings = VertexPaintSettings::Instance();

    scene::INodePtr targetNode = _strokeTarget.lock();
    if (!targetNode) return false;

    auto* paintable = Node_getVertexPaintable(targetNode);
    if (!paintable) return false;

    Ray ray;
    if (!buildRayFromEvent(ev, ray)) return false;

    Vector3 worldHit;
    if (!raycastTarget(ray, targetNode, worldHit)) return false;

    VertexPaintSettings::Instance().hoverValid = true;
    VertexPaintSettings::Instance().hoverPoint = worldHit;

    Matrix4 worldToLocal = targetNode->localToWorld().getFullInverse();
    Vector3 localCenter = worldToLocal.transformPoint(worldHit);

    bool touched = false;
    std::size_t count = paintable->getPaintableSurfaceCount();
    for (std::size_t i = 0; i < count; ++i)
    {
        if (auto* surf = paintable->getPaintableSurface(i))
        {
            if (vertexPaint::applyPaint(*surf, localCenter, settings.channel, settings))
            {
                touched = true;
            }
        }
    }

    if (touched)
    {
        paintable->queueRenderableUpdate();

        std::string modelPath = paintable->getPaintableModelPath();
        if (!modelPath.empty())
        {
            auto root = GlobalSceneGraph().root();
            if (root)
            {
                PaintPropagateWalker walker;
                walker.modelPath = modelPath;
                walker.localCenter = localCenter;
                walker.channel = settings.channel;
                walker.settings = &settings;
                walker.skipNode = targetNode;
                root->traverse(walker);
            }
        }
    }

    ev.getInteractiveView().queueDraw();
    return touched;
}

}
