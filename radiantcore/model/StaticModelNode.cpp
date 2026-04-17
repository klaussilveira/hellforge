#include "StaticModelNode.h"

#include "StaticModelSurface.h"
#include "ishaders.h"
#include "iscenegraph.h"

namespace model
{

StaticModelNode::StaticModelNode(const StaticModelPtr& picoModel) :
    _model(new StaticModel(*picoModel)),
    _name(picoModel->getFilename())
{
    _model->signal_ShadersChanged().connect(sigc::mem_fun(*this, &StaticModelNode::onModelShadersChanged));

    // Update the skin
    skinChanged("");
}

void StaticModelNode::createRenderableSurfaces()
{
    _model->foreachSurface([&](const StaticModelSurface& surface)
    {
        if (surface.getVertexArray().empty() || surface.getIndexArray().empty())
        {
            return; // don't handle empty surfaces
        }

        auto renderable = std::make_shared<RenderableModelSurface>(surface, _renderEntity, localToWorld());
        renderable->setPreviewMode(_previewMode);
        emplaceRenderableSurface(std::move(renderable));
    });
}

void StaticModelNode::onInsertIntoScene(scene::IMapRootNode& root)
{
    ModelNodeBase::onInsertIntoScene(root);
}

void StaticModelNode::onRemoveFromScene(scene::IMapRootNode& root)
{
    ModelNodeBase::onRemoveFromScene(root);
}

const IModel& StaticModelNode::getIModel() const
{
    return *_model;
}

IModel& StaticModelNode::getIModel()
{
    return *_model;
}

AABB StaticModelNode::localAABB() const {
    return _model->localAABB();
}

// SelectionTestable implementation
void StaticModelNode::testSelect(Selector& selector, SelectionTest& test) {
    _model->testSelect(selector, test, localToWorld());
}

std::string StaticModelNode::name() const {
    return _model->getFilename();
}

const StaticModelPtr& StaticModelNode::getModel() const {
    return _model;
}

void StaticModelNode::setModel(const StaticModelPtr& model) {
    _model = model;
}

void StaticModelNode::setRenderSystem(const RenderSystemPtr& renderSystem)
{
    ModelNodeBase::setRenderSystem(renderSystem);

    // This will trigger onModelShadersChanged() to refresh the renderables
    _model->setRenderSystem(renderSystem);
}

void StaticModelNode::onModelShadersChanged()
{
    // Detach renderables on model shader change,
    // they will be refreshed next time things are rendered
    detachFromShaders();
}

// Traceable implementation
bool StaticModelNode::getIntersection(const Ray& ray, Vector3& intersection)
{
    return _model->getIntersection(ray, intersection, localToWorld());
}

void StaticModelNode::skinChanged(const std::string& newSkinName)
{
    // The new skin name is stored locally
    _skin = newSkinName;

    // greebo: Acquire the ModelSkin reference from the SkinCache (might return null)
    // Applying the skin might trigger onModelShadersChanged()
    _model->applySkin(GlobalModelSkinCache().findSkin(getSkin()));

    // Refresh the scene (TODO: get rid of that)
    GlobalSceneGraph().sceneChanged();
}

std::string StaticModelNode::getSkin() const
{
    return !_skin.empty() ? _skin : _defaultSkin;
}

void StaticModelNode::setDefaultSkin(const std::string& defaultSkin)
{
    _defaultSkin = defaultSkin;
}

std::size_t StaticModelNode::getPaintableSurfaceCount()
{
    return static_cast<std::size_t>(_model->getSurfaceCount());
}

IIndexedModelSurface* StaticModelNode::getPaintableSurface(std::size_t index)
{
    if (index >= _model->getSurfaces().size()) return nullptr;
    return _model->getSurfaces()[index].surface.get();
}

std::string StaticModelNode::getPaintableModelPath() const
{
    return _model->getModelPath();
}

std::string StaticModelNode::getPaintableModelFilename() const
{
    return _model->getFilename();
}

void StaticModelNode::setPreviewMode(PaintablePreviewMode mode)
{
    if (_previewMode == mode) return;
    _previewMode = mode;
    applyPreviewModeToSurfaces(mode);
}

} // namespace model
