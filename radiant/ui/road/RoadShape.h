#pragma once

#include "RoadNetwork.h"

#include "math/Plane3.h"
#include "math/Vector2.h"
#include "math/Vector3.h"

#include <cstddef>
#include <string>
#include <vector>

namespace road
{

enum CurbStyle
{
    CURB_SQUARE = 0,
    CURB_ROUND = 1,
};

enum CornerStyle
{
    CORNER_SQUARE = 0,
    CORNER_ROUND = 1,
};

struct RoadParams
{
    int subdivisions = 8;
    int lanes = 2;
    double laneWidth = 128;
    bool sidewalk = true;
    double sidewalkWidth = 96;
    double sidewalkHeight = 16;
    int curbStyle = CURB_SQUARE;
    double curbRadius = 8;
    int cornerStyle = CORNER_ROUND;
    double cornerRadius = 64;
    double texScale = 1.0 / 128.0;
    std::string roadMaterial = "_default";
    std::string sidewalkMaterial = "_default";
    std::string curbMaterial = "_default";
    std::string hiddenMaterial = "textures/common/caulk";
};

struct BrushFace
{
    Plane3 plane;
    std::string material;
};

struct TextureFrame
{
    Vector3 origin = Vector3(0, 0, 0);
    Vector3 uAxis = Vector3(1, 0, 0);
    double scale = 1.0 / 128.0;
};

struct BrushSolid
{
    std::vector<BrushFace> faces;
};

struct RoadPlan
{
    std::vector<BrushSolid> brushes;
    TextureFrame frame;
};

struct FaceProjection
{
    Vector3 points[3];
    Vector2 uvs[3];
};

bool faceProjection(const Plane3& plane, const TextureFrame& frame, FaceProjection& result);

double roadHalfWidth(const RoadParams& params);

RoadPlan buildPlan(const std::vector<std::vector<Vector3>>& curves, const RoadParams& params);

} // namespace road
