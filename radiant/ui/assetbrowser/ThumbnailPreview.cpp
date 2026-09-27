#include "ThumbnailPreview.h"

#include "ieclass.h"
#include "ientity.h"
#include "imodel.h"
#include "ishaders.h"
#include "model/BestViewSolver.h"
#include "registry/registry.h"
#include "scene/EntityNode.h"
#include "scene/PrefabBoundsAccumulator.h"

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
    showEntityRoot();

    if (type == assetType::Prefab)
    {
        return showPrefab(name);
    }

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

bool ThumbnailPreview::showPrefab(const std::string& path)
{
    registry::ScopedKeyChanger<bool> changer(RKEY_MAP_SUPPRESS_LOAD_STATUS_DIALOG, true);

    try
    {
        auto resource = GlobalMapResourceManager().createFromPath(path);

        if (!resource || !resource->load()) return false;

        const auto& root = resource->getRootNode();

        scene::PrefabBoundsAccumulator accumulator;
        root->traverseChildren(accumulator);

        if (!accumulator.getBounds().isValid()) return false;

        setEntity(EntityNodePtr());

        _entityRoot = getScene()->root();
        _prefabResource = resource;
        _prefabBounds = accumulator.getBounds();

        getScene()->setRoot(root);
        associateRenderSystem();

        _assetViewAngles = model::getDefaultViewAngles();

        queueSceneUpdate();

        return true;
    }
    catch (const std::runtime_error&)
    {
        return false;
    }
}

void ThumbnailPreview::showEntityRoot()
{
    if (!_prefabResource) return;

    _prefabResource.reset();

    getScene()->setRoot(_entityRoot);
    associateRenderSystem();

    _entityRoot.reset();
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

bool ThumbnailPreview::onPreRender()
{
    if (!_prefabResource) return EntityPreview::onPreRender();

    prepareScene();

    return true;
}

AABB ThumbnailPreview::getSceneBounds()
{
    return _prefabResource ? _prefabBounds : EntityPreview::getSceneBounds();
}

bool ThumbnailPreview::canDrawGrid()
{
    return false;
}

void ThumbnailPreview::setupInitialViewPosition()
{
    if (!getEntity() && !_prefabResource) return;

    frameBounds(getSceneBounds(), _assetViewAngles, _padding);
}

}
