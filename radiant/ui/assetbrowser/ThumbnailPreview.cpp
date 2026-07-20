#include "ThumbnailPreview.h"

#include "ieclass.h"
#include "ientity.h"
#include "ishaders.h"
#include "scene/EntityNode.h"
#include "wxutil/GLWidget.h"

#include "AssetTypes.h"

namespace ui
{

ThumbnailPreview::ThumbnailPreview(wxWindow* parent) :
    EntityPreview(parent)
{}

bool ThumbnailPreview::showAsset(const std::string& type, const std::string& name)
{
    try
    {
        if (type == assetType::Model)
        {
            auto entity = GlobalEntityModule().createEntity(
                GlobalEntityClassManager().findClass("func_static"));

            entity->getEntity().setKeyValue("model", name);
            setEntity(entity);
            return true;
        }

        auto eclass = GlobalEntityClassManager().findClass(name);

        if (!eclass) return false;

        setEntity(GlobalEntityModule().createEntity(eclass));
        return true;
    }
    catch (const std::runtime_error&)
    {
        return false;
    }
}

bool ThumbnailPreview::captureImage(wxImage& image)
{
    auto collisionMaterial = GlobalMaterialManager().getMaterial("textures/common/collision");
    bool collisionWasVisible = collisionMaterial && collisionMaterial->isVisible();

    if (collisionWasVisible)
    {
        collisionMaterial->setVisible(false);
    }

    bool result = getGLWidget()->captureImage(image);

    if (collisionWasVisible)
    {
        collisionMaterial->setVisible(true);
    }

    return result;
}

bool ThumbnailPreview::canDrawGrid()
{
    return false;
}

void ThumbnailPreview::setupInitialViewPosition()
{
    if (!getEntity()) return;

    double distance = getSceneBounds().getRadius() * _defaultCamDistanceFactor;

    setViewOrigin(getSceneBounds().getOrigin() + Vector3(1, 1, 1) * distance);
    setViewAngles(Vector3(34, 135, 0));
}

}
