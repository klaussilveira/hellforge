#include "ThumbnailPreview.h"

#include "ieclass.h"
#include "ientity.h"
#include "imodel.h"
#include "ishaders.h"
#include "model/BestViewSolver.h"
#include "scene/EntityNode.h"

#include "AssetTypes.h"

namespace ui
{

namespace
{

const model::IModel* findModel(const scene::INodePtr& node)
{
    const model::IModel* result = nullptr;

    node->foreachNode([&](const scene::INodePtr& child)
    {
        auto modelNode = Node_getModel(child);

        if (!modelNode) return true;

        result = &modelNode->getIModel();
        return false;
    });

    return result;
}

}

ThumbnailPreview::ThumbnailPreview(wxWindow* parent) :
    EntityPreview(parent),
    _assetViewAngles(model::getDefaultViewAngles())
{}

bool ThumbnailPreview::showAsset(const std::string& type, const std::string& name)
{
    try
    {
        EntityNodePtr entity;

        if (type == assetType::Model)
        {
            entity = GlobalEntityModule().createEntity(
                GlobalEntityClassManager().findClass("func_static"));

            entity->getEntity().setKeyValue("model", name);
        }
        else
        {
            auto eclass = GlobalEntityClassManager().findClass(name);

            if (!eclass) return false;

            entity = GlobalEntityModule().createEntity(eclass);
        }

        setEntity(entity);

        auto* assetModel = findModel(entity);

        _assetViewAngles = assetModel != nullptr
            ? model::calculateBestViewAngles(*assetModel)
            : model::getDefaultViewAngles();

        return true;
    }
    catch (const std::runtime_error&)
    {
        return false;
    }
}

bool ThumbnailPreview::captureImage(wxImage& image, int size)
{
    auto collisionMaterial = GlobalMaterialManager().getMaterial("textures/common/collision");
    bool collisionWasVisible = collisionMaterial && collisionMaterial->isVisible();

    if (collisionWasVisible)
    {
        collisionMaterial->setVisible(false);
    }

    bool result = renderToImage(image, size);

    if (collisionWasVisible)
    {
        collisionMaterial->setVisible(true);
    }

    return result;
}

void ThumbnailPreview::setPadding(float padding)
{
    _padding = padding;
}

const Vector3& ThumbnailPreview::getAssetViewAngles() const
{
    return _assetViewAngles;
}

void ThumbnailPreview::setAssetViewAngles(const Vector3& angles)
{
    _assetViewAngles = angles;

    queueSceneUpdate();
}

bool ThumbnailPreview::canDrawGrid()
{
    return false;
}

void ThumbnailPreview::setupInitialViewPosition()
{
    if (!getEntity()) return;

    frameBounds(getSceneBounds(), _assetViewAngles, _padding);
}

}
