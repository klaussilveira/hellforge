#include "PlayerClearanceTool.h"

#include "ibrush.h"
#include "ishaders.h"
#include "gamelib.h"
#include "scene/Entity.h"
#include "string/predicate.h"

#include <cmath>

namespace ui
{

namespace playerClearance
{

namespace
{
    constexpr double FloorOffset = 0.25;
    constexpr double PlaneEpsilon = 0.01;

    bool brushBlocksPlayer(const IBrush& brush)
    {
        for (std::size_t i = 0; i < brush.getNumFaces(); ++i)
        {
            if (materialBlocksPlayer(brush.getFace(i).getShader()))
            {
                return true;
            }
        }

        return false;
    }

    bool hullIntersectsBrush(const AABB& hull, const IBrush& brush)
    {
        for (std::size_t i = 0; i < brush.getNumFaces(); ++i)
        {
            const Plane3& plane = brush.getFace(i).getPlane3();
            const Vector3& n = plane.normal();

            double support = n.dot(hull.origin)
                - std::abs(n.x()) * hull.extents.x()
                - std::abs(n.y()) * hull.extents.y()
                - std::abs(n.z()) * hull.extents.z();

            if (support >= plane.dist() - PlaneEpsilon)
            {
                return false;
            }
        }

        return true;
    }

    class ClearanceWalker :
        public scene::NodeVisitor
    {
    private:
        AABB _hull;
        bool _blocked = false;

    public:
        ClearanceWalker(const AABB& hull) :
            _hull(hull)
        {}

        bool isBlocked() const
        {
            return _blocked;
        }

        bool pre(const scene::INodePtr& node) override
        {
            if (_blocked)
            {
                return false;
            }

            if (auto entity = node->tryGetEntity())
            {
                if (string::starts_with(entity->getKeyValue("classname"), "trigger_"))
                {
                    return false;
                }

                return entity->getKeyValue("solid") != "0";
            }

            IBrush* brush = Node_getIBrush(node);

            if (brush == nullptr)
            {
                return true;
            }

            if (node->worldAABB().intersects(_hull) && brushBlocksPlayer(*brush) && hullIntersectsBrush(_hull, *brush))
            {
                _blocked = true;
            }

            return false;
        }
    };

    bool hullFits(const scene::INodePtr& root, const AABB& hull)
    {
        ClearanceWalker walker(hull);
        root->traverse(walker);
        return !walker.isBlocked();
    }
}

Dimensions loadDimensions()
{
    Dimensions dims;
    dims.width = game::current::getValue<double>("/player/width");
    dims.standHeight = game::current::getValue<double>("/player/standHeight");
    dims.crouchHeight = game::current::getValue<double>("/player/crouchHeight");
    dims.standViewHeight = game::current::getValue<double>("/player/standViewHeight");
    dims.crouchViewHeight = game::current::getValue<double>("/player/crouchViewHeight");
    dims.minWalkNormal = game::current::getValue<double>("/player/minWalkNormal");
    return dims;
}

bool materialBlocksPlayer(const std::string& material)
{
    auto shader = GlobalMaterialManager().getMaterial(material);

    if (!shader)
    {
        return true;
    }

    int flags = shader->getSurfaceFlags();

    if (flags & (Material::SURF_SOLID | Material::SURF_PLAYERCLIP))
    {
        return true;
    }

    return (flags & (Material::SURF_NONSOLID | Material::SURF_WATER | Material::SURF_AREAPORTAL | Material::SURF_NOCARVE)) == 0;
}

AABB hullAt(const Vector3& origin, double width, double height)
{
    return AABB(origin + Vector3(0, 0, height * 0.5), Vector3(width * 0.5, width * 0.5, height * 0.5));
}

bool placeOnFloor(const Vector3& point, const Vector3& normal, const Dimensions& dims, Vector3& origin)
{
    if (normal.z() <= 0 || normal.z() < dims.minWalkNormal)
    {
        return false;
    }

    double lift = dims.width * 0.5 * (std::abs(normal.x()) + std::abs(normal.y())) / normal.z();
    origin = point + Vector3(0, 0, lift + FloorOffset);
    return true;
}

Fit classify(const scene::INodePtr& root, const Vector3& origin, const Dimensions& dims)
{
    if (hullFits(root, hullAt(origin, dims.width, dims.standHeight)))
    {
        return Fit::Stand;
    }

    if (hullFits(root, hullAt(origin, dims.width, dims.crouchHeight)))
    {
        return Fit::Crouch;
    }

    return Fit::Blocked;
}

}

}
