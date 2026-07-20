#include "AssetDropTarget.h"

#include <cmath>
#include <cstring>

#include "ientity.h"
#include "imap.h"
#include "iorthoview.h"
#include "iscenegraph.h"
#include "iselection.h"
#include "itransformable.h"
#include "iundo.h"
#include "command/ExecutionFailure.h"
#include "scene/EntityNode.h"
#include "scenelib.h"
#include "selection/Device.h"
#include "string/predicate.h"

#include "AssetTypes.h"
#include "camera/tools/FaceIntersectionFinder.h"

namespace ui
{

namespace
{
constexpr const char* ASSET_PAYLOAD_PREFIX = "hellforge_asset|";
}

std::string makeAssetDragPayload(const std::string& type, const std::string& name)
{
    return std::string(ASSET_PAYLOAD_PREFIX) + type + "|" + name;
}

AssetDropTarget::AssetDropTarget(IInteractiveView& view, FallbackPointFunc fallback) :
    _view(view),
    _fallback(std::move(fallback))
{}

bool AssetDropTarget::OnDropText(wxCoord x, wxCoord y, const wxString& text)
{
    auto payload = text.ToStdString();

    if (!string::starts_with(payload, ASSET_PAYLOAD_PREFIX)) return false;

    auto rest = payload.substr(std::strlen(ASSET_PAYLOAD_PREFIX));
    auto separator = rest.find('|');

    if (separator == std::string::npos) return false;

    auto type = rest.substr(0, separator);
    auto name = rest.substr(separator + 1);

    if (name.empty()) return false;
    if (type != assetType::Model && type != assetType::EntityClass) return false;
    if (!GlobalMapModule().getRoot()) return false;

    auto test = _view.createSelectionTestForPoint(device_constrained(window_to_normalised_device(
        Vector2(x, y), _view.getDeviceWidth(), _view.getDeviceHeight())));

    FaceIntersectionFinder finder(*test, test->getVolume().GetViewProjection());
    GlobalSceneGraph().root()->traverse(finder);

    auto intersection = finder.getResult();

    Vector3 position;

    if (intersection.valid)
    {
        position = intersection.point;

        if (auto orthoView = dynamic_cast<IOrthoView*>(&_view))
        {
            orthoView->snapToGrid(position);
        }
    }
    else
    {
        position = _fallback(x, y, finder.getRayOrigin() + finder.getRayDirection() * 256.0);
    }

    UndoableCommand command("dropAsset");

    GlobalSelectionSystem().setSelectedAll(false);

    EntityNodePtr node;

    try
    {
        if (type == assetType::Model)
        {
            node = GlobalEntityModule().createModelEntityFromSelection(name, position);
        }
        else
        {
            node = GlobalEntityModule().createEntityFromSelection(name, position);
        }
    }
    catch (const cmd::ExecutionFailure&)
    {
        return false;
    }

    if (node && intersection.valid)
    {
        auto aabb = node->worldAABB();

        if (aabb.isValid())
        {
            const auto& normal = intersection.normal;

            Vector3 absNormal(std::abs(normal.x()), std::abs(normal.y()), std::abs(normal.z()));
            double penetration = (aabb.origin - position).dot(normal) - aabb.extents.dot(absNormal);

            if (penetration < -0.01)
            {
                auto transform = scene::node_cast<ITransformable>(node);

                if (transform)
                {
                    transform->setType(TRANSFORM_PRIMITIVE);
                    transform->setTranslation(normal * -penetration);
                    transform->freezeTransform();
                }
            }
        }
    }

    return true;
}

}
