#pragma once

#include <vector>

#include "imodel.h"
#include "math/Matrix4.h"
#include "math/Vector3.h"
#include "polygon/Polygon2D.h"

namespace brush
{

namespace algorithm
{

struct OpeningFrame
{
    Vector3 origin;
    Vector3 normal;
    Vector3 run;
    Vector3 up;
    double back = 0;
    double front = 0;
};

struct OpeningSettings
{
    double cell = 1.0;
    double simplify = 1.0;
    double snap = 2.0;
    double faceTolerance = 0.5;
    double minPieceArea = 1.0;
    double apertureFraction = 0.5;
    double minOpeningArea = 64.0;
    double maxGrowth = 0.08;
    std::size_t maxCellsPerAxis = 512;
};

struct OpeningSolution
{
    bool valid = false;
    std::vector<polygon::Ring> pieces;
    std::vector<polygon::Ring> outline;
    std::vector<polygon::Ring> ignored;
    double area = 0;
    double leakArea = 0;
    double modelDepth = 0;
    std::size_t ignoredParts = 0;
};

OpeningSolution solveOpening(const model::IModel& model, const Matrix4& modelToWorld,
    const OpeningFrame& frame, const OpeningSettings& settings);

OpeningFrame buildOpeningFrame(const Vector3& normal, const Vector3& pointOnMidPlane,
    double back, double front);

} // namespace algorithm

} // namespace brush
