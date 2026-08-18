#pragma once

#include "ibrush.h"
#include "scenelib.h"
#include "gamelib.h"
#include "math/AABB.h"
#include "math/Matrix3.h"
#include "math/Plane3.h"
#include "math/Vector3.h"

#include <cmath>
#include <string>
#include <vector>

namespace roof
{

enum RoofType
{
    ROOF_GABLE = 0,
    ROOF_HIP = 1,
};

enum RidgeAxis
{
    RIDGE_AUTO = 0,
    RIDGE_ALONG_X = 1,
    RIDGE_ALONG_Y = 2,
};

struct RoofParams
{
    int type = ROOF_GABLE;
    int ridgeAxis = RIDGE_AUTO;
    double height = 96;
    double slabThickness = 8;
    double eave = 16;
    double rake = 8;
    std::string material = "_default";
};

struct RoofFace
{
    Plane3 underside;
    std::vector<Plane3> sides;
};

inline double getTextureScale()
{
    return game::current::getValue<double>("/generators/texScale", 1.0 / 128.0);
}

inline Plane3 axisPlane(int axis, double sign, double value)
{
    Vector3 normal(0, 0, 0);
    normal[axis] = sign;

    return Plane3(normal, sign * value);
}

inline Plane3 slopePlane(int axis, double sign, double edge, double baseZ, double slope)
{
    double length = std::sqrt(slope * slope + 1.0);

    Vector3 normal(0, 0, 1.0 / length);
    normal[axis] = sign * slope / length;

    Vector3 point(0, 0, baseZ);
    point[axis] = edge;

    return Plane3(normal, normal.dot(point));
}

inline Plane3 bisectorPlane(int axisA, double signA, double edgeA,
                            int axisB, double signB, double edgeB)
{
    Vector3 normal(0, 0, 0);
    normal[axisA] -= signA;
    normal[axisB] += signB;

    double dist = signB * edgeB - signA * edgeA;
    double length = normal.getLength();

    return Plane3(normal / length, dist / length);
}

inline bool ridgeRunsAlongX(const AABB& footprint, int ridgeAxis)
{
    if (ridgeAxis == RIDGE_ALONG_X)
    {
        return true;
    }

    if (ridgeAxis == RIDGE_ALONG_Y)
    {
        return false;
    }

    return footprint.extents.x() >= footprint.extents.y();
}

inline bool spansFootprint(const AABB& bounds, const AABB& footprint)
{
    return bounds.extents.x() >= footprint.extents.x() * 0.9 &&
           bounds.extents.y() >= footprint.extents.y() * 0.9;
}

inline std::vector<AABB> selectWalls(const std::vector<AABB>& selected)
{
    AABB selectionBounds;

    for (const AABB& bounds : selected)
    {
        selectionBounds.includeAABB(bounds);
    }

    std::vector<AABB> walls;

    for (const AABB& bounds : selected)
    {
        if (!spansFootprint(bounds, selectionBounds))
        {
            walls.push_back(bounds);
        }
    }

    return walls.empty() ? selected : walls;
}

inline AABB footprintOf(const std::vector<AABB>& walls)
{
    AABB footprint;

    for (const AABB& bounds : walls)
    {
        footprint.includeAABB(bounds);
    }

    return footprint;
}

inline std::vector<RoofFace> buildRoofFaces(const AABB& footprint, const RoofParams& params)
{
    Vector3 mins = footprint.origin - footprint.extents;
    Vector3 maxs = footprint.origin + footprint.extents;

    bool alongX = ridgeRunsAlongX(footprint, params.ridgeAxis);
    int slopeAxis = alongX ? 1 : 0;
    int ridgeAxis = alongX ? 0 : 1;

    double baseZ = maxs.z();
    double slope = params.height / footprint.extents[slopeAxis];

    struct Side
    {
        int axis;
        double sign;
        double edge;
        double outerEdge;
    };

    std::vector<Side> sides;
    sides.push_back({slopeAxis, 1.0, maxs[slopeAxis], maxs[slopeAxis] + params.eave});
    sides.push_back({slopeAxis, -1.0, mins[slopeAxis], mins[slopeAxis] - params.eave});

    if (params.type == ROOF_HIP)
    {
        sides.push_back({ridgeAxis, 1.0, maxs[ridgeAxis], maxs[ridgeAxis] + params.eave});
        sides.push_back({ridgeAxis, -1.0, mins[ridgeAxis], mins[ridgeAxis] - params.eave});
    }

    std::vector<RoofFace> faces;

    for (std::size_t i = 0; i < sides.size(); ++i)
    {
        RoofFace face;
        face.underside = slopePlane(sides[i].axis, sides[i].sign, sides[i].edge, baseZ, slope);
        face.sides.push_back(axisPlane(sides[i].axis, sides[i].sign, sides[i].outerEdge));

        for (std::size_t j = 0; j < sides.size(); ++j)
        {
            if (i == j)
            {
                continue;
            }

            face.sides.push_back(bisectorPlane(
                sides[i].axis, sides[i].sign, sides[i].edge,
                sides[j].axis, sides[j].sign, sides[j].edge));
        }

        if (params.type == ROOF_GABLE)
        {
            face.sides.push_back(axisPlane(ridgeAxis, 1.0, maxs[ridgeAxis] + params.rake));
            face.sides.push_back(axisPlane(ridgeAxis, -1.0, mins[ridgeAxis] - params.rake));
        }

        faces.push_back(face);
    }

    return faces;
}

inline std::vector<Plane3> slabPlanes(const RoofFace& face, double thickness)
{
    std::vector<Plane3> planes = face.sides;

    planes.push_back(-face.underside);

    Plane3 top = face.underside;
    top.dist() += top.normal().z() * thickness;
    planes.push_back(top);

    return planes;
}

inline std::vector<Plane3> connectorPlanes(const AABB& wall, const std::vector<RoofFace>& faces)
{
    Vector3 mins = wall.origin - wall.extents;
    Vector3 maxs = wall.origin + wall.extents;

    std::vector<Plane3> planes;
    planes.push_back(axisPlane(0, 1.0, maxs.x()));
    planes.push_back(axisPlane(0, -1.0, mins.x()));
    planes.push_back(axisPlane(1, 1.0, maxs.y()));
    planes.push_back(axisPlane(1, -1.0, mins.y()));
    planes.push_back(axisPlane(2, -1.0, maxs.z()));

    for (const RoofFace& face : faces)
    {
        planes.push_back(face.underside);
    }

    return planes;
}

inline scene::INodePtr createBrushFromPlanes(
    const std::vector<Plane3>& planes, const std::string& material,
    const scene::INodePtr& parent)
{
    auto brushNode = GlobalBrushCreator().createBrush();
    auto& brush = *Node_getIBrush(brushNode);

    double texScale = getTextureScale();
    Matrix3 proj = Matrix3::getIdentity();
    proj.xx() = texScale;
    proj.yy() = texScale;

    for (const Plane3& plane : planes)
    {
        brush.addFace(plane, proj, material);
    }

    brush.evaluateBRep();
    brush.removeEmptyFaces();

    if (!brush.hasContributingFaces())
    {
        return scene::INodePtr();
    }

    scene::addNodeToContainer(brushNode, parent);

    return brushNode;
}

inline std::vector<scene::INodePtr> generateRoof(
    const std::vector<AABB>& walls, const AABB& footprint,
    const RoofParams& params, const scene::INodePtr& parent)
{
    std::vector<scene::INodePtr> result;

    if (!footprint.isValid() || footprint.extents.x() < 1 || footprint.extents.y() < 1 ||
        params.height < 1 || params.slabThickness < 1)
    {
        return result;
    }

    auto faces = buildRoofFaces(footprint, params);

    for (const AABB& wall : walls)
    {
        auto node = createBrushFromPlanes(connectorPlanes(wall, faces), params.material, parent);

        if (node)
        {
            result.push_back(node);
        }
    }

    for (const RoofFace& face : faces)
    {
        auto node = createBrushFromPlanes(
            slabPlanes(face, params.slabThickness), params.material, parent);

        if (node)
        {
            result.push_back(node);
        }
    }

    return result;
}

} // namespace roof
