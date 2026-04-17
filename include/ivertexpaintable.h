#pragma once

#include "inode.h"
#include "imodelsurface.h"
#include <cstddef>
#include <string>

namespace model
{

enum class PaintablePreviewMode
{
    Material,
    VertexColour
};

class IVertexPaintable
{
public:
    virtual ~IVertexPaintable() = default;

    virtual std::size_t getPaintableSurfaceCount() = 0;
    virtual IIndexedModelSurface* getPaintableSurface(std::size_t index) = 0;

    virtual void queueRenderableUpdate() = 0;

    virtual std::string getPaintableModelPath() const = 0;
    virtual std::string getPaintableModelFilename() const = 0;

    virtual void setPreviewMode(PaintablePreviewMode mode) = 0;
};

}

inline model::IVertexPaintable* Node_getVertexPaintable(const scene::INodePtr& node)
{
    return dynamic_cast<model::IVertexPaintable*>(node.get());
}
