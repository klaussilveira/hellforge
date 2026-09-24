#pragma once

#include "ishaders.h"
#include "ishaderlayer.h"

namespace shaders
{

inline IShaderLayer::Ptr findFirstLayerOfType(const MaterialPtr& material, IShaderLayer::Type type)
{
    IShaderLayer::Ptr found;

    material->foreachLayer([&](const IShaderLayer::Ptr& layer)
    {
        if (layer->getType() == type)
        {
            found = layer;
            return false;
        }

        return true;
    });

    return found;
}

}
