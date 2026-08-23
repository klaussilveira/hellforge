#pragma once

#include "RoadShape.h"

#include "ibrush.h"
#include "gamelib.h"
#include "scenelib.h"

#include "math/Matrix3.h"
#include "math/Vector2.h"
#include "math/Vector3.h"

#include <cstddef>
#include <vector>

namespace road
{

inline double getTextureScale()
{
    return game::current::getValue<double>("/generators/texScale", 1.0 / 128.0);
}

inline void applyFrameProjection(IBrush& brush, const TextureFrame& frame)
{
    for (std::size_t index = 0; index < brush.getNumFaces(); ++index)
    {
        IFace& face = brush.getFace(index);

        FaceProjection projection;

        if (!faceProjection(face.getPlane3(), frame, projection))
        {
            continue;
        }

        face.setTexDefFromPoints(projection.points, projection.uvs);
        face.freezeTransform();
    }
}

inline scene::INodePtr createBrushNode(const BrushSolid& solid, const TextureFrame& frame,
                                       const scene::INodePtr& parent)
{
    if (solid.faces.size() < 4)
    {
        return scene::INodePtr();
    }

    auto brushNode = GlobalBrushCreator().createBrush();
    auto& brush = *Node_getIBrush(brushNode);

    Matrix3 projection = Matrix3::getIdentity();
    projection.xx() = frame.scale;
    projection.yy() = frame.scale;

    for (const BrushFace& face : solid.faces)
    {
        brush.addFace(face.plane, projection, face.material);
    }

    brush.evaluateBRep();
    brush.removeEmptyFaces();

    if (!brush.hasContributingFaces())
    {
        return scene::INodePtr();
    }

    applyFrameProjection(brush, frame);

    scene::addNodeToContainer(brushNode, parent);

    return brushNode;
}

inline std::vector<scene::INodePtr> generateRoad(const std::vector<std::vector<Vector3>>& curves,
                                                 const RoadParams& params,
                                                 const scene::INodePtr& parent)
{
    std::vector<scene::INodePtr> result;

    RoadPlan plan = buildPlan(curves, params);

    for (const BrushSolid& solid : plan.brushes)
    {
        auto node = createBrushNode(solid, plan.frame, parent);

        if (node)
        {
            result.push_back(node);
        }
    }

    return result;
}

} // namespace road
