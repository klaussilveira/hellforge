#pragma once

#include "ibrush.h"
#include "math/Vector2.h"
#include <optional>
#include <string>
#include <vector>

namespace ui
{

namespace wallgeometry
{

struct WallJointCap
{
    Vector2 normal;
    double dist;
};

struct WallJointCaps
{
    WallJointCap segmentCap;
    WallJointCap otherCap;
};

std::optional<WallJointCaps> computeButtJointCaps(const Vector2& corner,
    const Vector2& segmentAway, double segmentThickness,
    const Vector2& otherAway, double otherThickness);

Vector2 snapSegmentEnd(const Vector2& anchor, const Vector2& current, double gridSize);

void buildWallSegmentFaces(IBrush& brush, const Vector2& a, const Vector2& b,
    double baseZ, double height, double thickness, const std::string& material,
    const std::optional<WallJointCap>& capA = {}, const std::optional<WallJointCap>& capB = {});

double distanceToSegment(const Vector2& point, const Vector2& a, const Vector2& b);

void buildPrismFaces(IBrush& brush, const std::vector<Vector2>& polygon,
    double zBottom, double zTop, const std::string& material);

void buildShedRoofFaces(IBrush& brush, const Vector2& mins, const Vector2& maxs,
    double baseZ, double rise, const std::string& material);

void buildGabledRoofWedgeFaces(IBrush& brush, const Vector2& mins, const Vector2& maxs,
    double baseZ, double rise, bool firstHalf, const std::string& material);

std::vector<Vector2> simplifyCollinear(const std::vector<Vector2>& polygon);

double signedArea(const std::vector<Vector2>& polygon);

bool isConvex(const std::vector<Vector2>& polygon);

bool isAxisAlignedRectangle(const std::vector<Vector2>& polygon);

std::vector<std::vector<Vector2>> decomposeIntoConvex(const std::vector<Vector2>& polygon);

}

}
