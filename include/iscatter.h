#pragma once

namespace ui
{

enum class ScatterDensityMethod
{
    Density = 0,
    Amount = 1
};

enum class ScatterDistribution
{
    Random = 0,
    PoissonDisk = 1
};

enum class ScatterBrushMode
{
    AreaBoundary = 0,
    Surface = 1
};

enum class ScatterFaceDirection
{
    FacingCamera = 0,
    PositiveX = 1,
    NegativeX = 2,
    PositiveY = 3,
    NegativeY = 4,
    PositiveZ = 5,
    NegativeZ = 6
};

} // namespace
