#pragma once

#include "inode.h"
#include "iselectiontest.h"
#include "math/Vector3.h"
#include "math/Plane3.h"
#include "math/Matrix4.h"

namespace ui
{

/**
 * Result of a face intersection test.
 */
struct FaceIntersection
{
    bool valid = false;
    Vector3 point;
    Vector3 normal;
    scene::INodePtr node;
};

/**
 * Scene visitor that finds the closest brush face intersection
 * for a given selection test (ray cast from camera).
 */
class FaceIntersectionFinder :
    public scene::NodeVisitor
{
private:
    SelectionTest& _selectionTest;
    SelectionIntersection _bestIntersection;

    Vector3 _worldRayOrigin;
    Vector3 _worldRayDirection;

    bool _hasResult = false;
    Plane3 _bestPlane;
    scene::INodePtr _bestNode;

public:
    FaceIntersectionFinder(SelectionTest& test, const Matrix4& viewProjection);

    bool pre(const scene::INodePtr& node) override;

    FaceIntersection getResult() const;

    const Vector3& getRayOrigin() const
    {
        return _worldRayOrigin;
    }

    const Vector3& getRayDirection() const
    {
        return _worldRayDirection;
    }

private:
    static void computeWorldRay(const Matrix4& viewProjection,
                                 Vector3& outOrigin, Vector3& outDirection);
};

} // namespace ui
