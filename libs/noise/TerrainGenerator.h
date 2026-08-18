#pragma once

#include "Noise.h"

#include "inode.h"
#include "ipatch.h"
#include "math/Vector3.h"

#include <cstddef>
#include <string>

namespace noise
{

inline scene::INodePtr generateTerrainPatch(
    const NoiseParameters& params,
    std::size_t columns, std::size_t rows,
    float physicalWidth, float physicalHeight,
    const Vector3& spawnPos, const std::string& material,
    const scene::INodePtr& parent)
{
    scene::INodePtr node = GlobalPatchModule().createPatch(patch::PatchDefType::Def2);

    parent->addChildNode(node);

    IPatch* patch = Node_getIPatch(node);

    if (!patch)
    {
        return scene::INodePtr();
    }

    patch->setDims(columns, rows);

    NoiseGenerator noiseGen(params);
    float spacingX = physicalWidth / static_cast<float>(columns - 1);
    float spacingY = physicalHeight / static_cast<float>(rows - 1);
    float offsetX = static_cast<float>(spawnPos.x()) - physicalWidth / 2.0f;
    float offsetY = static_cast<float>(spawnPos.y()) - physicalHeight / 2.0f;
    float baseZ = static_cast<float>(spawnPos.z());

    for (std::size_t row = 0; row < rows; ++row)
    {
        for (std::size_t col = 0; col < columns; ++col)
        {
            PatchControl& ctrl = patch->ctrlAt(row, col);

            float worldX = offsetX + col * spacingX;
            float worldY = offsetY + row * spacingY;
            float noiseValue = static_cast<float>(noiseGen.sample(worldX, worldY));

            ctrl.vertex.x() = worldX;
            ctrl.vertex.y() = worldY;
            ctrl.vertex.z() = baseZ + noiseValue;
            ctrl.texcoord.x() = static_cast<float>(col) / static_cast<float>(columns - 1);
            ctrl.texcoord.y() = static_cast<float>(row) / static_cast<float>(rows - 1);
        }
    }

    patch->controlPointsChanged();
    patch->setShader(material);
    patch->scaleTextureNaturally();

    return node;
}

} // namespace noise
