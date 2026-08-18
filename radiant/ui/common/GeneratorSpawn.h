#pragma once

#include "icameraview.h"
#include "igrid.h"
#include "math/Vector3.h"

#include <stdexcept>

namespace ui
{

inline Vector3 getGeneratorSpawnPosition(double distance = 256.0)
{
    try
    {
        auto& camera = GlobalCameraManager().getActiveView();

        return (camera.getCameraOrigin() - camera.getForwardVector() * distance)
            .getSnapped(GlobalGrid().getGridSize());
    }
    catch (const std::runtime_error&)
    {
    }

    return Vector3(0, 0, 0);
}

} // namespace ui
