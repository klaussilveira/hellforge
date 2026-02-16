#include "StaticModel.h"
#include "StaticModelSurface.h"

#include "ivolumetest.h"
#include "iselectiontest.h"
#include "ishaders.h"
#include "modelskin.h"
#include "imodelsurface.h"
#include "VolumeIntersectionValue.h"
#include "math/Ray.h"

namespace model
{

StaticModel::StaticModel(const std::vector<StaticModelSurfacePtr>& surfaces)
{
    for (const auto& surface : surfaces)
    {
        auto& inserted = _surfaces.emplace_back(surface);

        // Extend the model AABB to include each surface's AABB
        _localAABB.includeAABB(inserted.surface->getAABB());
    }
}

StaticModel::StaticModel(const StaticModel& other) :
    _surfaces(other._surfaces.size()),
    _localAABB(other._localAABB),
    _filename(other._filename),
    _modelPath(other._modelPath)
{
    // Copy the other model's surfaces, but not its shaders, revert to default
    for (std::size_t i = 0; i < other._surfaces.size(); ++i)
    {
        // Copy-construct the other surface, inheriting any applied scale
        _surfaces[i].surface = std::make_shared<StaticModelSurface>(*(other._surfaces[i].surface));
        _surfaces[i].originalSurface = other._surfaces[i].originalSurface;
        _surfaces[i].surface->setActiveMaterial(_surfaces[i].surface->getDefaultMaterial());
    }
}

void StaticModel::setRenderSystem(const RenderSystemPtr& renderSystem)
{
    _renderSystem = renderSystem;

    captureShaders();
}

std::string StaticModel::getFilename() const 
{
    return _filename;
}

void StaticModel::setFilename(const std::string& name)
{
    _filename = name;
}

// Return vertex count of this model
int StaticModel::getVertexCount() const 
{
    int sum = 0;

    for (const Surface& s : _surfaces)
    {
        sum += s.surface->getNumVertices();
    }

    return sum;
}

// Return poly count of this model
int StaticModel::getPolyCount() const
{
    int sum = 0;

    for (const Surface& s : _surfaces)
    {
        sum += s.surface->getNumTriangles();
    }

    return sum;
}

const IModelSurface& StaticModel::getSurface(unsigned surfaceNum) const
{
    assert(surfaceNum >= 0 && surfaceNum < _surfaces.size());
    return *(_surfaces[surfaceNum].surface);
}

// Apply the given skin to this model
void StaticModel::applySkin(const decl::ISkin::Ptr& skin)
{
    // Apply the skin to each surface, then try to capture shaders
    for (auto& s : _surfaces)
    {
        const std::string& defaultMaterial = s.surface->getDefaultMaterial();
        const std::string& activeMaterial = s.surface->getActiveMaterial();

        // Look up the remap for this surface's material name. If there is a remap
        // change the Shader* to point to the new shader.
        auto remap = skin ? skin->getRemap(defaultMaterial) : std::string();

        if (!remap.empty() && remap != activeMaterial)
        {
            // Save the remapped shader name
            s.surface->setActiveMaterial(remap);
        }
        else if (remap.empty() && activeMaterial != defaultMaterial)
        {
            // No remap, so reset our shader to the original unskinned shader
            s.surface->setActiveMaterial(defaultMaterial);
        }
    }

    captureShaders();

    // greebo: Update the active material list after applying this skin
    updateMaterialList();
}

void StaticModel::captureShaders()
{
    auto renderSystem = _renderSystem.lock();

    // Capture or release our shaders
    for (auto& s : _surfaces)
    {
        if (renderSystem)
        {
            s.shader = renderSystem->capture(s.surface->getActiveMaterial());
        }
        else
        {
            s.shader.reset();
        }
    }

    _sigShadersChanged.emit();
}

sigc::signal<void>& StaticModel::signal_ShadersChanged()
{
    return _sigShadersChanged;
}

// Update the list of active materials
void StaticModel::updateMaterialList() const
{
    _materialList.clear();

    for (const auto& s : _surfaces)
    {
        _materialList.push_back(s.surface->getActiveMaterial());
    }
}

// Return the list of active skins for this model
const StringList& StaticModel::getActiveMaterials() const
{
    // If the material list is empty, populate it
    if (_materialList.empty())
    {
        updateMaterialList();
    }

    // Return the list
    return _materialList;
}

void StaticModel::testSelect(Selector& selector, SelectionTest& test, const Matrix4& localToWorld)
{
    // Perform a volume intersection (AABB) check on each surface. For those
    // that intersect, call the surface's own testSelection method to perform
    // a proper selection test.
    for (const auto& surface : _surfaces)
    {
        // Check volume intersection
        if (test.getVolume().TestAABB(surface.surface->getAABB(), localToWorld) != VOLUME_OUTSIDE)
        {
            bool twoSided = surface.shader && surface.shader->getMaterial()->getCullType() == Material::CULL_NONE;

            // Volume intersection passed, delegate the selection test
            surface.surface->testSelect(selector, test, localToWorld, twoSided);
        }
    }
}

bool StaticModel::getIntersection(const Ray& ray, Vector3& intersection, const Matrix4& localToWorld)
{
    Vector3 bestIntersection = ray.origin;

    // Test each surface and take the nearest point to the ray origin
    for (SurfaceList::iterator i = _surfaces.begin(); i != _surfaces.end(); ++i)
    {
        Vector3 surfaceIntersection;

        if (i->surface->getIntersection(ray, surfaceIntersection, localToWorld))
        {
            // Test if this surface intersection is better than what we currently have
            auto oldDistSquared = (bestIntersection - ray.origin).getLengthSquared();
            auto newDistSquared = (surfaceIntersection - ray.origin).getLengthSquared();

            if ((oldDistSquared == 0 && newDistSquared > 0) || newDistSquared < oldDistSquared)
            {
                bestIntersection = surfaceIntersection;
            }
        }
    }

    if ((bestIntersection - ray.origin).getLengthSquared() > 0)
    {
        intersection = bestIntersection;
        return true;
    }
    else
    {
        return false;
    }
}

const StaticModel::SurfaceList& StaticModel::getSurfaces() const
{
    return _surfaces;
}

std::string StaticModel::getModelPath() const
{
    return _modelPath;
}

void StaticModel::setModelPath(const std::string& modelPath)
{
    _modelPath = modelPath;
}

void StaticModel::foreachSurface(const std::function<void(const StaticModelSurface&)>& func) const
{
    for (const Surface& surf : _surfaces)
    {
        func(*surf.surface);
    }
}

} // namespace
