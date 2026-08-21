#include "RadiantTest.h"

#include "ibrush.h"
#include "ientity.h"
#include "imap.h"
#include "iselection.h"
#include "iundo.h"
#include "scenelib.h"
#include "math/AABB.h"
#include "math/Vector3.h"

#include "algorithm/Entity.h"
#include "algorithm/Primitives.h"
#include "algorithm/Scene.h"

#include "ui/cut/CutGeometry.h"

#include <algorithm>
#include <vector>

namespace test
{

using CutGeometryTest = RadiantTest;

namespace
{

const double BOUNDS_TOLERANCE = 0.0001;

std::vector<scene::INodePtr> collectBrushes(const scene::INodePtr& parent)
{
    std::vector<scene::INodePtr> result;

    parent->foreachNode([&](const scene::INodePtr& child)
    {
        if (Node_getIBrush(child) != nullptr)
        {
            result.push_back(child);
        }

        return true;
    });

    return result;
}

void expectSpanStarts(const std::vector<scene::INodePtr>& brushes, int axis,
                      const std::vector<double>& expected)
{
    std::vector<double> actual;

    for (const scene::INodePtr& brush : brushes)
    {
        AABB bounds = brush->worldAABB();
        actual.push_back(bounds.origin[axis] - bounds.extents[axis]);
    }

    std::sort(actual.begin(), actual.end());

    ASSERT_EQ(actual.size(), expected.size());

    for (std::size_t i = 0; i < expected.size(); ++i)
    {
        EXPECT_NEAR(actual[i], expected[i], BOUNDS_TOLERANCE);
    }
}

void expectExtents(const scene::INodePtr& node, const Vector3& expected)
{
    AABB bounds = node->worldAABB();

    EXPECT_NEAR(bounds.extents.x(), expected.x(), BOUNDS_TOLERANCE);
    EXPECT_NEAR(bounds.extents.y(), expected.y(), BOUNDS_TOLERANCE);
    EXPECT_NEAR(bounds.extents.z(), expected.z(), BOUNDS_TOLERANCE);
}

std::size_t countContributingFaces(const scene::INodePtr& node)
{
    IBrush* brush = Node_getIBrush(node);

    std::size_t count = 0;

    for (std::size_t i = 0; i < brush->getNumFaces(); ++i)
    {
        if (brush->getFace(i).getWinding().size() >= 3)
        {
            ++count;
        }
    }

    return count;
}

cut::CutParams equalParts(int parts, int axis)
{
    cut::CutParams params;
    params.axis = axis;
    params.rule.type = cut::RULE_EQUAL_PARTS;
    params.rule.parts = parts;

    return params;
}

} // anonymous namespace

TEST_F(CutGeometryTest, HalfCutProducesTwoAbuttingSlabs)
{
    auto worldspawn = GlobalMapModule().findOrInsertWorldspawn();

    // 256 wide on X, centred on the origin, so it spans [-128, 128]
    auto source = algorithm::createCuboidBrush(worldspawn,
        AABB(Vector3(0, 0, 0), Vector3(128, 32, 32)));

    cut::applyCuts({ cut::snapshotBrush(source) }, equalParts(2, cut::AXIS_X));

    auto brushes = collectBrushes(worldspawn);
    ASSERT_EQ(brushes.size(), 2);

    expectSpanStarts(brushes, cut::AXIS_X, { -128, 0 });

    for (const scene::INodePtr& slab : brushes)
    {
        expectExtents(slab, Vector3(64, 32, 32));
    }
}

TEST_F(CutGeometryTest, ThirdsProduceThreeEqualSlabs)
{
    auto worldspawn = GlobalMapModule().findOrInsertWorldspawn();

    // 96 wide on X, spanning [-48, 48]
    auto source = algorithm::createCuboidBrush(worldspawn,
        AABB(Vector3(0, 0, 0), Vector3(48, 32, 32)));

    cut::applyCuts({ cut::snapshotBrush(source) }, equalParts(3, cut::AXIS_X));

    auto brushes = collectBrushes(worldspawn);
    ASSERT_EQ(brushes.size(), 3);

    expectSpanStarts(brushes, cut::AXIS_X, { -48, -16, 16 });
}

TEST_F(CutGeometryTest, SlabsRemainClosedSixSidedBoxes)
{
    auto worldspawn = GlobalMapModule().findOrInsertWorldspawn();

    auto source = algorithm::createCuboidBrush(worldspawn,
        AABB(Vector3(0, 0, 0), Vector3(128, 32, 32)));

    cut::applyCuts({ cut::snapshotBrush(source) }, equalParts(4, cut::AXIS_X));

    auto brushes = collectBrushes(worldspawn);
    ASSERT_EQ(brushes.size(), 4);

    for (const scene::INodePtr& slab : brushes)
    {
        EXPECT_EQ(countContributingFaces(slab), 6);
    }
}

TEST_F(CutGeometryTest, SlabsInheritTheSourceMaterial)
{
    auto worldspawn = GlobalMapModule().findOrInsertWorldspawn();

    auto source = algorithm::createCuboidBrush(worldspawn,
        AABB(Vector3(0, 0, 0), Vector3(128, 32, 32)), "textures/numbers/1");

    cut::applyCuts({ cut::snapshotBrush(source) }, equalParts(2, cut::AXIS_X));

    auto brushes = collectBrushes(worldspawn);
    ASSERT_EQ(brushes.size(), 2);

    for (const scene::INodePtr& slab : brushes)
    {
        IBrush* brush = Node_getIBrush(slab);

        for (std::size_t i = 0; i < brush->getNumFaces(); ++i)
        {
            EXPECT_EQ(brush->getFace(i).getShader(), "textures/numbers/1");
        }
    }
}

TEST_F(CutGeometryTest, LongestAxisFollowsTheBrushShapeNotTheAxisOrder)
{
    auto worldspawn = GlobalMapModule().findOrInsertWorldspawn();

    // Deepest on Y (512 units), so the longest axis must be Y, not X
    auto source = algorithm::createCuboidBrush(worldspawn,
        AABB(Vector3(0, 0, 0), Vector3(32, 256, 64)));

    cut::applyCuts({ cut::snapshotBrush(source) }, equalParts(2, cut::AXIS_LONGEST));

    auto brushes = collectBrushes(worldspawn);
    ASSERT_EQ(brushes.size(), 2);

    expectSpanStarts(brushes, cut::AXIS_Y, { -256, 0 });

    // X and Z must have been left whole
    for (const scene::INodePtr& slab : brushes)
    {
        expectExtents(slab, Vector3(32, 128, 64));
    }
}

TEST_F(CutGeometryTest, SlabsStayInsideTheSourceEntity)
{
    auto worldspawn = GlobalMapModule().findOrInsertWorldspawn();

    scene::INodePtr funcStatic = algorithm::createEntityByClassName("func_static");
    scene::addNodeToContainer(funcStatic, GlobalMapModule().getRoot());

    auto source = algorithm::createCuboidBrush(funcStatic,
        AABB(Vector3(0, 0, 0), Vector3(128, 32, 32)));

    cut::applyCuts({ cut::snapshotBrush(source) }, equalParts(2, cut::AXIS_X));

    EXPECT_EQ(collectBrushes(worldspawn).size(), 0);

    auto brushes = collectBrushes(funcStatic);
    ASSERT_EQ(brushes.size(), 2);

    for (const scene::INodePtr& slab : brushes)
    {
        EXPECT_EQ(slab->getParent(), funcStatic);
    }
}

TEST_F(CutGeometryTest, EntityOriginDoesNotShiftTheCut)
{
    // A brush-based func_static keeps an identity transform and bakes any move into
    // its children, so the origin spawnarg must not displace the cut.
    auto funcStatic = algorithm::createEntityByClassName("func_static");
    funcStatic->getEntity().setKeyValue("origin", "512 0 0");

    scene::INodePtr entityNode = funcStatic;
    scene::addNodeToContainer(entityNode, GlobalMapModule().getRoot());

    auto source = algorithm::createCuboidBrush(entityNode,
        AABB(Vector3(0, 0, 0), Vector3(128, 32, 32)));

    cut::applyCuts({ cut::snapshotBrush(source) }, equalParts(2, cut::AXIS_X));

    auto brushes = collectBrushes(entityNode);
    ASSERT_EQ(brushes.size(), 2);

    // Still the brush's own midpoint, not shifted by 512
    expectSpanStarts(brushes, cut::AXIS_X, { -128, 0 });

    for (const scene::INodePtr& slab : brushes)
    {
        expectExtents(slab, Vector3(64, 32, 32));
    }
}

TEST_F(CutGeometryTest, UndoRestoresTheUncutBrush)
{
    auto worldspawn = GlobalMapModule().findOrInsertWorldspawn();

    auto source = algorithm::createCuboidBrush(worldspawn,
        AABB(Vector3(0, 0, 0), Vector3(128, 32, 32)));

    cut::applyCuts({ cut::snapshotBrush(source) }, equalParts(4, cut::AXIS_X));

    ASSERT_EQ(collectBrushes(worldspawn).size(), 4);

    GlobalUndoSystem().undo();

    auto brushes = collectBrushes(worldspawn);
    ASSERT_EQ(brushes.size(), 1);

    expectExtents(brushes.front(), Vector3(128, 32, 32));
}

TEST_F(CutGeometryTest, ARuleYieldingNoCutsLeavesTheBrushAlone)
{
    auto worldspawn = GlobalMapModule().findOrInsertWorldspawn();

    auto source = algorithm::createCuboidBrush(worldspawn,
        AABB(Vector3(0, 0, 0), Vector3(128, 32, 32)));

    cut::applyCuts({ cut::snapshotBrush(source) }, equalParts(1, cut::AXIS_X));

    auto brushes = collectBrushes(worldspawn);
    ASSERT_EQ(brushes.size(), 1);

    EXPECT_EQ(brushes.front(), source);
}

TEST_F(CutGeometryTest, SpacingCutsEveryTenUnitsWithOneBetween)
{
    auto worldspawn = GlobalMapModule().findOrInsertWorldspawn();

    // Spans [0, 40] on X
    auto source = algorithm::createCuboidBrush(worldspawn,
        AABB(Vector3(20, 0, 0), Vector3(20, 32, 32)));

    cut::CutParams params;
    params.axis = cut::AXIS_X;
    params.rule.type = cut::RULE_SPACING;
    params.rule.step = 10;
    params.rule.subdivisions = 1;

    cut::applyCuts({ cut::snapshotBrush(source) }, params);

    auto brushes = collectBrushes(worldspawn);
    ASSERT_EQ(brushes.size(), 8);

    expectSpanStarts(brushes, cut::AXIS_X, { 0, 5, 10, 15, 20, 25, 30, 35 });
}

TEST_F(CutGeometryTest, PatternAlternatesSegmentWidths)
{
    auto worldspawn = GlobalMapModule().findOrInsertWorldspawn();

    // Spans [0, 45] on X
    auto source = algorithm::createCuboidBrush(worldspawn,
        AABB(Vector3(22.5, 0, 0), Vector3(22.5, 32, 32)));

    cut::CutParams params;
    params.axis = cut::AXIS_X;
    params.rule.type = cut::RULE_PATTERN;
    params.rule.pattern = { 10, 5 };

    cut::applyCuts({ cut::snapshotBrush(source) }, params);

    auto brushes = collectBrushes(worldspawn);
    ASSERT_EQ(brushes.size(), 6);

    expectSpanStarts(brushes, cut::AXIS_X, { 0, 10, 15, 25, 30, 40 });
}

} // namespace test
