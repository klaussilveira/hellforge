#pragma once

#include "ibrush.h"
#include "iselection.h"
#include "iundo.h"
#include "scenelib.h"
#include "math/AABB.h"
#include "math/Matrix3.h"
#include "math/Plane3.h"
#include "math/Vector3.h"

#include "CutRules.h"

#include "string/case_conv.h"

#include <map>
#include <memory>
#include <string>
#include <vector>

namespace cut
{

enum AxisType
{
    AXIS_X = 0,
    AXIS_Y = 1,
    AXIS_Z = 2,
    AXIS_LONGEST = 3,
};

const int AXIS_COUNT = 3;

struct CutFace
{
    Plane3 plane;
    Matrix3 projection;
    std::string material;
};

struct CutSource
{
    std::vector<CutFace> faces;
    AABB bounds;
    std::string dominantMaterial;
    Matrix3 dominantProjection = Matrix3::getIdentity();
    scene::INodePtr node;
    scene::INodePtr parent;
};

struct CutParams
{
    int axis = AXIS_X;
    CutRule rule;
    bool overrideMaterial = false;
    std::string material;
};

inline CutSource snapshotBrush(const scene::INodePtr& node)
{
    CutSource source;
    source.node = node;
    source.parent = node->getParent();
    source.bounds = node->worldAABB();

    IBrush* brush = Node_getIBrush(node);

    if (brush == nullptr)
    {
        return source;
    }

    std::map<std::string, std::size_t> materialCount;
    std::size_t dominantCount = 0;

    for (std::size_t i = 0; i < brush->getNumFaces(); ++i)
    {
        IFace& face = brush->getFace(i);

        CutFace cutFace;
        cutFace.plane = face.getPlane3();
        cutFace.projection = face.getProjectionMatrix();
        cutFace.material = face.getShader();

        source.faces.push_back(cutFace);

        std::size_t count = ++materialCount[cutFace.material];

        if (count > dominantCount)
        {
            dominantCount = count;
            source.dominantMaterial = cutFace.material;
            source.dominantProjection = cutFace.projection;
        }
    }

    return source;
}

inline int axisFromString(const std::string& input)
{
    std::string value = string::to_lower_copy(input);

    if (value == "x")
    {
        return AXIS_X;
    }

    if (value == "y")
    {
        return AXIS_Y;
    }

    if (value == "z")
    {
        return AXIS_Z;
    }

    return AXIS_LONGEST;
}

inline int resolveAxis(int axis, const AABB& bounds)
{
    if (axis != AXIS_LONGEST)
    {
        return axis;
    }

    int longest = 0;

    for (int i = 1; i < AXIS_COUNT; ++i)
    {
        if (bounds.extents[i] > bounds.extents[longest])
        {
            longest = i;
        }
    }

    return longest;
}

inline Plane3 axisPlane(int axis, double sign, double value)
{
    Vector3 normal(0, 0, 0);
    normal[axis] = sign;

    return Plane3(normal, sign * value);
}

inline double axisMin(const AABB& bounds, int axis)
{
    return bounds.origin[axis] - bounds.extents[axis];
}

inline double axisMax(const AABB& bounds, int axis)
{
    return bounds.origin[axis] + bounds.extents[axis];
}

inline std::vector<double> positionsForSource(const CutSource& source, const CutParams& params)
{
    int axis = resolveAxis(params.axis, source.bounds);

    return computeCutPositions(params.rule, axisMin(source.bounds, axis),
                               axisMax(source.bounds, axis));
}

inline std::vector<Plane3> cutPlanesForSource(const CutSource& source, const CutParams& params)
{
    int axis = resolveAxis(params.axis, source.bounds);

    std::vector<Plane3> planes;

    for (double position : positionsForSource(source, params))
    {
        planes.push_back(axisPlane(axis, 1, position));
    }

    return planes;
}

inline void setCutPreview(const std::vector<CutSource>& sources, const CutParams& params)
{
    for (const CutSource& source : sources)
    {
        auto brushNode = std::dynamic_pointer_cast<IBrushNode>(source.node);

        if (brushNode)
        {
            brushNode->setCutPlanes(cutPlanesForSource(source, params));
        }
    }
}

inline void clearCutPreview(const std::vector<CutSource>& sources)
{
    for (const CutSource& source : sources)
    {
        auto brushNode = std::dynamic_pointer_cast<IBrushNode>(source.node);

        if (brushNode)
        {
            brushNode->setCutPlanes({});
        }
    }
}

inline scene::INodePtr createSlab(const CutSource& source, const CutParams& params, int axis,
                                  double lower, double upper, bool hasLower, bool hasUpper)
{
    auto brushNode = GlobalBrushCreator().createBrush();

    IBrush& brush = *Node_getIBrush(brushNode);

    for (const CutFace& face : source.faces)
    {
        brush.addFace(face.plane, face.projection, face.material);
    }

    const std::string& cutMaterial =
        params.overrideMaterial ? params.material : source.dominantMaterial;

    if (hasUpper)
    {
        brush.addFace(axisPlane(axis, 1, upper), source.dominantProjection, cutMaterial);
    }

    if (hasLower)
    {
        brush.addFace(axisPlane(axis, -1, lower), source.dominantProjection, cutMaterial);
    }

    brush.evaluateBRep();
    brush.removeEmptyFaces();

    if (!brush.hasContributingFaces())
    {
        return scene::INodePtr();
    }

    brushNode->assignToLayers(source.node->getLayers());

    scene::addNodeToContainer(brushNode, source.parent);

    return brushNode;
}

inline std::vector<scene::INodePtr> generateCuts(const std::vector<CutSource>& sources,
                                                 const CutParams& params)
{
    std::vector<scene::INodePtr> result;

    for (const CutSource& source : sources)
    {
        int axis = resolveAxis(params.axis, source.bounds);

        double min = axisMin(source.bounds, axis);
        double max = axisMax(source.bounds, axis);

        std::vector<double> positions = computeCutPositions(params.rule, min, max);

        double lower = min;

        for (std::size_t i = 0; i <= positions.size(); ++i)
        {
            bool hasUpper = i < positions.size();
            double upper = hasUpper ? positions[i] : max;

            auto slab = createSlab(source, params, axis, lower, upper, i > 0, hasUpper);

            if (slab)
            {
                result.push_back(slab);
            }

            lower = upper;
        }
    }

    return result;
}

inline std::size_t countCuts(const std::vector<CutSource>& sources, const CutParams& params)
{
    std::size_t total = 0;

    for (const CutSource& source : sources)
    {
        total += positionsForSource(source, params).size();
    }

    return total;
}

inline void applyCuts(const std::vector<CutSource>& sources, const CutParams& params)
{
    if (countCuts(sources, params) == 0)
    {
        return;
    }

    UndoableCommand undo("cutBrush");

    GlobalSelectionSystem().setSelectedAll(false);

    for (const CutSource& source : sources)
    {
        scene::removeNodeFromParent(source.node);
    }

    for (const scene::INodePtr& slab : generateCuts(sources, params))
    {
        Node_setSelected(slab, true);
    }

    SceneChangeNotify();
}

} // namespace cut
