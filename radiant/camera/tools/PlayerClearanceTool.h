#pragma once

#include "imousetool.h"
#include "inode.h"
#include "irenderable.h"
#include "irender.h"
#include "math/AABB.h"
#include "render/RenderableColouredBoundingBoxes.h"

#include <string>
#include <vector>

namespace ui
{

namespace playerClearance
{
    struct Dimensions
    {
        double width = 0;
        double standHeight = 0;
        double crouchHeight = 0;
        double standViewHeight = 0;
        double crouchViewHeight = 0;
        double minWalkNormal = 0;

        bool isValid() const
        {
            return width > 0 && standHeight > 0 && crouchHeight > 0;
        }
    };

    enum class Fit
    {
        None,
        Stand,
        Crouch,
        Blocked
    };

    Dimensions loadDimensions();
    bool materialBlocksPlayer(const std::string& material);
    AABB hullAt(const Vector3& origin, double width, double height);
    bool placeOnFloor(const Vector3& point, const Vector3& normal, const Dimensions& dims, Vector3& origin);
    Fit classify(const scene::INodePtr& root, const Vector3& origin, const Dimensions& dims);
}

class PlayerClearanceTool :
    public MouseTool,
    public Renderable
{
private:
    bool _enabled = false;
    playerClearance::Dimensions _dims;
    playerClearance::Fit _fit = playerClearance::Fit::None;
    Vector3 _origin;

    std::vector<AABB> _hullBoxes;
    std::vector<AABB> _eyeBoxes;
    std::vector<Vector4> _colours;
    render::RenderableColouredBoundingBoxes _hull;
    render::RenderableColouredBoundingBoxes _eye;

    ShaderPtr _shader;

public:
    PlayerClearanceTool();

    const std::string& getName() override;
    const std::string& getDisplayName() override;

    Result onMouseDown(Event& ev) override;
    Result onMouseMove(Event& ev) override;
    Result onMouseUp(Event& ev) override;
    void onMouseLeave(IInteractiveView& view) override;

    bool alwaysReceivesMoveEvents() override { return true; }

    void setEnabled(bool enabled);

    void setRenderSystem(const RenderSystemPtr&) override {}
    void onPreRender(const VolumeTest& volume) override;
    void renderHighlights(IRenderableCollector&, const VolumeTest&) override {}
    std::size_t getHighlightFlags() override { return Highlight::NoHighlight; }

private:
    void clearGeometry();
};

}
