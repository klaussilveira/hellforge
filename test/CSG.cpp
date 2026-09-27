#include "RadiantTest.h"

#include "imap.h"
#include "igrid.h"
#include <set>
#include "ibrush.h"
#include "entitylib.h"
#include "algorithm/Scene.h"

namespace test
{

using CsgTest = RadiantTest;

TEST_F(CsgTest, CSGMergeTwoRegularWorldspawnBrushes)
{
    loadMap("csg_merge.map");

    // Locate the first worldspawn brush
    auto worldspawn = GlobalMapModule().getWorldspawn();

    // Try to merge the two brushes with the "1" and "2" materials
    auto firstBrush = algorithm::findFirstBrushWithMaterial(worldspawn, "1");
    auto secondBrush = algorithm::findFirstBrushWithMaterial(worldspawn, "2");

    ASSERT_TRUE(Node_getIBrush(firstBrush)->getNumFaces() == 5);
    ASSERT_TRUE(Node_getIBrush(secondBrush)->getNumFaces() == 5);

    // Select the brushes and merge them
    GlobalSelectionSystem().setSelectedAll(false);
    Node_setSelected(firstBrush, true);
    Node_setSelected(secondBrush, true);

    // CSG merge
    GlobalCommandSystem().executeCommand("CSGMerge");

    // The two brushes should be gone, replaced by a new one
    ASSERT_TRUE(firstBrush->getParent() == nullptr);
    ASSERT_TRUE(secondBrush->getParent() == nullptr);

    // The merged brush will carry both materials
    auto brushWithMaterial1 = algorithm::findFirstBrushWithMaterial(worldspawn, "1");
    auto brushWithMaterial2 = algorithm::findFirstBrushWithMaterial(worldspawn, "2");

    ASSERT_TRUE(brushWithMaterial1 == brushWithMaterial2);
    ASSERT_TRUE(Node_getIBrush(brushWithMaterial1)->getNumFaces() == 6);
}

TEST_F(CsgTest, CSGMergeFourRegularWorldspawnBrushes)
{
    loadMap("csg_merge.map");

    // Locate the first worldspawn brush
    auto worldspawn = GlobalMapModule().getWorldspawn();

    // Try to merge the two brushes with the "1" and "2" materials
    std::vector<scene::INodePtr> brushes = {
        algorithm::findFirstBrushWithMaterial(worldspawn, "1"),
        algorithm::findFirstBrushWithMaterial(worldspawn, "2"),
        algorithm::findFirstBrushWithMaterial(worldspawn, "3"),
        algorithm::findFirstBrushWithMaterial(worldspawn, "4")
    };

    // Check the correct setup
    for (const auto& brush : brushes)
    {
        ASSERT_TRUE(Node_getIBrush(brush)->getNumFaces() == 5);
    }

    // Select the brushes and merge them
    GlobalSelectionSystem().setSelectedAll(false);
    for (const auto& brush : brushes)
    {
        Node_setSelected(brush, true);
    }

    // CSG merge
    GlobalCommandSystem().executeCommand("CSGMerge");

    // All brushes should be gone, replaced by a new one
    for (const auto& brush : brushes)
    {
        ASSERT_TRUE(brush->getParent() == nullptr);
    }

    // The combined brush should be a 6-sided cuboid
    auto brushWithMaterial1 = algorithm::findFirstBrushWithMaterial(worldspawn, "1");
    ASSERT_TRUE(Node_getIBrush(brushWithMaterial1)->getNumFaces() == 6);
}

TEST_F(CsgTest, CSGMergeTwoFuncStaticBrushes)
{
    loadMap("csg_merge.map");

    // Locate the func_static in the map
    EntityNodeFindByClassnameWalker walker("func_static");
    GlobalSceneGraph().root()->traverse(walker);

    auto entity = walker.getEntityNode();

    // Try to merge the two brushes with the "1" and "2" materials
    auto firstBrush = algorithm::findFirstBrushWithMaterial(entity, "1");
    auto secondBrush = algorithm::findFirstBrushWithMaterial(entity, "2");

    ASSERT_TRUE(Node_getIBrush(firstBrush)->getNumFaces() == 5);
    ASSERT_TRUE(Node_getIBrush(secondBrush)->getNumFaces() == 5);

    // Select the brushes and merge them
    GlobalSelectionSystem().setSelectedAll(false);
    Node_setSelected(firstBrush, true);
    Node_setSelected(secondBrush, true);

    // CSG merge
    GlobalCommandSystem().executeCommand("CSGMerge");

    // The two brushes should be gone, replaced by a new one
    ASSERT_TRUE(firstBrush->getParent() == nullptr);
    ASSERT_TRUE(secondBrush->getParent() == nullptr);

    // The merged brush will carry both materials
    auto brushWithMaterial1 = algorithm::findFirstBrushWithMaterial(entity, "1");
    auto brushWithMaterial2 = algorithm::findFirstBrushWithMaterial(entity, "2");

    ASSERT_TRUE(brushWithMaterial1 == brushWithMaterial2);
    ASSERT_TRUE(Node_getIBrush(brushWithMaterial1)->getNumFaces() == 6);

    // They should still be children of the same entity
    ASSERT_TRUE(brushWithMaterial1->getParent() == entity);
    ASSERT_TRUE(brushWithMaterial2->getParent() == entity);
}

// #5344: Check that selecting a couple of brushes will only merge those
// which share the same parent entity
TEST_F(CsgTest, CSGMergeBrushesOfMixedEntitySelection)
{
    loadMap("csg_merge.map");

    // Locate the func_static in the map
    EntityNodeFindByClassnameWalker walker("func_static");
    GlobalSceneGraph().root()->traverse(walker);

    auto entity = walker.getEntityNode();
    auto worldspawn = GlobalMapModule().getWorldspawn();

    // Select the mergeable brushes of both entities carrying the "1" and "2" materials
    std::vector<scene::INodePtr> brushes = {
        algorithm::findFirstBrushWithMaterial(entity, "1"),
        algorithm::findFirstBrushWithMaterial(entity, "2"),
        algorithm::findFirstBrushWithMaterial(worldspawn, "1"),
        algorithm::findFirstBrushWithMaterial(worldspawn, "2")
    };

    // Check the correct setup
    for (const auto& brush : brushes)
    {
        ASSERT_TRUE(Node_getIBrush(brush)->getNumFaces() == 5);
    }

    // Select the brushes and merge them
    GlobalSelectionSystem().setSelectedAll(false);

    for (const auto& brush : brushes)
    {
        Node_setSelected(brush, true);
    }

    // CSG merge
    GlobalCommandSystem().executeCommand("CSGMerge");

    // All brushes should be gone, replaced by TWO new ones
    for (const auto& brush : brushes)
    {
        ASSERT_TRUE(brush->getParent() == nullptr);
    }

    // The merged brush will carry both materials
    auto funcBrush1 = algorithm::findFirstBrushWithMaterial(entity, "1");
    auto funcBrush2 = algorithm::findFirstBrushWithMaterial(entity, "2");

    ASSERT_TRUE(funcBrush1);
    ASSERT_TRUE(funcBrush1 == funcBrush2);
    ASSERT_TRUE(Node_getIBrush(funcBrush1)->getNumFaces() == 6);

    // Same for the worldspawn entity
    auto worldBrush1 = algorithm::findFirstBrushWithMaterial(worldspawn, "1");
    auto worldBrush2 = algorithm::findFirstBrushWithMaterial(worldspawn, "2");

    ASSERT_TRUE(worldBrush1);
    ASSERT_TRUE(worldBrush1 == worldBrush2);
    ASSERT_TRUE(Node_getIBrush(worldBrush1)->getNumFaces() == 6);
}

// Issue #5336: Crash when using CSG Merge on brushes that are part of worldspawn and a func_static
TEST_F(CsgTest, CSGMergeWithFuncStatic)
{
    loadMap("csg_merge_with_func_static.map");

    // Locate the first worldspawn brush
    auto firstBrush = algorithm::getNthChild(GlobalMapModule().getWorldspawn(), 0);
    ASSERT_TRUE(firstBrush);

    // Locate the func_static in the map
    EntityNodeFindByClassnameWalker walker("func_static");
    GlobalSceneGraph().root()->traverse(walker);

    auto entityNode = walker.getEntityNode();
    ASSERT_TRUE(entityNode);
    ASSERT_TRUE(entityNode->hasChildNodes());

    // Select both of them, the order is important
    Node_setSelected(firstBrush, true);
    Node_setSelected(entityNode, true);

    // CSG merge
    GlobalCommandSystem().executeCommand("CSGMerge");

    // No merge should have happened since the brushes 
    // are not part of the same entity
    // So assume the scene didn't change
    ASSERT_TRUE(algorithm::getNthChild(GlobalMapModule().getWorldspawn(), 0) == firstBrush);

    EntityNodeFindByClassnameWalker walker2("func_static");
    GlobalSceneGraph().root()->traverse(walker2);

    ASSERT_TRUE(walker.getEntityNode());
    ASSERT_TRUE(walker.getEntityNode()->hasChildNodes());
}

TEST_F(CsgTest, CSGIntersectTwoOverlappingBrushes)
{
    loadMap("csg_intersect.map");

    auto worldspawn = GlobalMapModule().getWorldspawn();

    // Find the two overlapping brushes with materials "1" and "2"
    auto firstBrush = algorithm::findFirstBrushWithMaterial(worldspawn, "1");
    auto secondBrush = algorithm::findFirstBrushWithMaterial(worldspawn, "2");

    ASSERT_TRUE(firstBrush != nullptr);
    ASSERT_TRUE(secondBrush != nullptr);
    ASSERT_TRUE(Node_getIBrush(firstBrush)->getNumFaces() == 6);
    ASSERT_TRUE(Node_getIBrush(secondBrush)->getNumFaces() == 6);

    // Select the brushes and intersect them
    GlobalSelectionSystem().setSelectedAll(false);
    Node_setSelected(firstBrush, true);
    Node_setSelected(secondBrush, true);

    // CSG intersect
    GlobalCommandSystem().executeCommand("CSGIntersect");

    // The two brushes should be gone, replaced by a new one
    ASSERT_TRUE(firstBrush->getParent() == nullptr);
    ASSERT_TRUE(secondBrush->getParent() == nullptr);

    // The intersection should have created a new brush
    // It should have materials from the first brush (since we started with that)
    auto resultBrush = algorithm::findFirstBrushWithMaterial(worldspawn, "1");
    ASSERT_TRUE(resultBrush != nullptr);

    // The result should be a valid 6-sided brush (the intersection of two cubes is a cube)
    ASSERT_TRUE(Node_getIBrush(resultBrush)->getNumFaces() == 6);
}

TEST_F(CsgTest, CSGIntersectNonOverlappingBrushes)
{
    loadMap("csg_intersect.map");

    auto worldspawn = GlobalMapModule().getWorldspawn();

    // Find brush "1" and the non-overlapping brush "3"
    auto firstBrush = algorithm::findFirstBrushWithMaterial(worldspawn, "1");
    auto nonOverlappingBrush = algorithm::findFirstBrushWithMaterial(worldspawn, "3");

    ASSERT_TRUE(firstBrush != nullptr);
    ASSERT_TRUE(nonOverlappingBrush != nullptr);

    // Select the brushes
    GlobalSelectionSystem().setSelectedAll(false);
    Node_setSelected(firstBrush, true);
    Node_setSelected(nonOverlappingBrush, true);

    // CSG intersect - should fail silently because brushes don't overlap
    GlobalCommandSystem().executeCommand("CSGIntersect");

    // The original brushes should still exist (operation failed, no changes)
    ASSERT_TRUE(firstBrush->getParent() != nullptr);
    ASSERT_TRUE(nonOverlappingBrush->getParent() != nullptr);
}

TEST_F(CsgTest, CSGIntersectContainedBrush)
{
    loadMap("csg_intersect.map");

    auto worldspawn = GlobalMapModule().getWorldspawn();

    // Find brush "1" (large) and brush "4" (small, contained within "1")
    auto largeBrush = algorithm::findFirstBrushWithMaterial(worldspawn, "1");
    auto smallBrush = algorithm::findFirstBrushWithMaterial(worldspawn, "4");

    ASSERT_TRUE(largeBrush != nullptr);
    ASSERT_TRUE(smallBrush != nullptr);
    ASSERT_TRUE(Node_getIBrush(largeBrush)->getNumFaces() == 6);
    ASSERT_TRUE(Node_getIBrush(smallBrush)->getNumFaces() == 6);

    // Select the brushes
    GlobalSelectionSystem().setSelectedAll(false);
    Node_setSelected(largeBrush, true);
    Node_setSelected(smallBrush, true);

    // CSG intersect
    GlobalCommandSystem().executeCommand("CSGIntersect");

    // The two brushes should be gone
    ASSERT_TRUE(largeBrush->getParent() == nullptr);
    ASSERT_TRUE(smallBrush->getParent() == nullptr);

    // The result should be a brush equal in size to the small brush
    // The result will have material "4" since those faces define the intersection volume
    auto resultBrush = algorithm::findFirstBrushWithMaterial(worldspawn, "4");
    ASSERT_TRUE(resultBrush != nullptr);
    ASSERT_TRUE(Node_getIBrush(resultBrush)->getNumFaces() == 6);
}

TEST_F(CsgTest, CSGIntersectRequiresTwoBrushes)
{
    loadMap("csg_intersect.map");

    auto worldspawn = GlobalMapModule().getWorldspawn();

    // Find just one brush
    auto brush = algorithm::findFirstBrushWithMaterial(worldspawn, "1");
    ASSERT_TRUE(brush != nullptr);

    // Select only one brush
    GlobalSelectionSystem().setSelectedAll(false);
    Node_setSelected(brush, true);

    // CSG intersect - should fail silently because we need at least 2 brushes
    GlobalCommandSystem().executeCommand("CSGIntersect");

    // The original brush should still exist (operation failed, no changes)
    ASSERT_TRUE(brush->getParent() != nullptr);
}

namespace
{

std::vector<AABB> getBrushBoundsInside(const scene::INodePtr& worldspawn, const AABB& region)
{
    std::vector<AABB> result;

    worldspawn->foreachNode([&](const scene::INodePtr& node)
    {
        if (Node_isBrush(node) && region.contains(node->worldAABB()))
        {
            result.push_back(node->worldAABB());
        }

        return true;
    });

    return result;
}

bool boundsContainPoint(const std::vector<AABB>& bounds, const Vector3& point)
{
    for (const AABB& aabb : bounds)
    {
        if (aabb.intersects(point))
        {
            return true;
        }
    }

    return false;
}

std::set<std::string> getBrushMaterialsInside(const scene::INodePtr& worldspawn, const AABB& region)
{
    std::set<std::string> result;

    worldspawn->foreachNode([&](const scene::INodePtr& node)
    {
        if (Node_isBrush(node) && region.contains(node->worldAABB()))
        {
            IBrush* brush = Node_getIBrush(node);

            for (std::size_t i = 0; i < brush->getNumFaces(); ++i)
            {
                result.insert(brush->getFace(i).getShader());
            }
        }

        return true;
    });

    return result;
}

std::vector<AABB> getBrushBoundsWithMaterial(const scene::INodePtr& worldspawn, const std::string& material)
{
    std::vector<AABB> result;

    worldspawn->foreachNode([&](const scene::INodePtr& node)
    {
        if (Node_isBrush(node) && Node_getIBrush(node)->hasShader(material))
        {
            result.push_back(node->worldAABB());
        }

        return true;
    });

    return result;
}

void runCarve(const std::vector<scene::INodePtr>& nodes)
{
    GlobalSelectionSystem().setSelectedAll(false);

    for (const scene::INodePtr& node : nodes)
    {
        Node_setSelected(node, true);
    }

    GlobalCommandSystem().executeCommand("CarveEntityOpening");
}

void expectDoorwayCut(const scene::INodePtr& worldspawn)
{
    auto pieces = getBrushBoundsInside(worldspawn,
        AABB::createFromMinMax(Vector3(123, -97, -1), Vector3(133, 161, 129)));

    EXPECT_EQ(pieces.size(), 3);
    EXPECT_FALSE(boundsContainPoint(pieces, Vector3(128, 30, 48)));
    EXPECT_FALSE(boundsContainPoint(pieces, Vector3(128, 3, 95)));
    EXPECT_FALSE(boundsContainPoint(pieces, Vector3(128, 57, 1)));
    EXPECT_TRUE(boundsContainPoint(pieces, Vector3(128, 1, 48)));
    EXPECT_TRUE(boundsContainPoint(pieces, Vector3(128, 59, 48)));
    EXPECT_TRUE(boundsContainPoint(pieces, Vector3(128, 30, 97)));
    EXPECT_FALSE(boundsContainPoint(pieces, Vector3(128, 1.75, 48)));
    EXPECT_FALSE(boundsContainPoint(pieces, Vector3(128, 58.25, 48)));
    EXPECT_FALSE(boundsContainPoint(pieces, Vector3(128, 30, 96.25)));

    auto materials = getBrushMaterialsInside(worldspawn,
        AABB::createFromMinMax(Vector3(123, -97, -1), Vector3(133, 161, 129)));

    EXPECT_EQ(materials, std::set<std::string>({ "wall" }));
}

}

TEST_F(CsgTest, CarveOpeningWithWallSelected)
{
    loadMap("carve_opening.map");

    auto worldspawn = GlobalMapModule().getWorldspawn();
    auto wall = algorithm::findFirstBrushWithMaterial(worldspawn, "wall");
    auto floor = algorithm::findFirstBrushWithMaterial(worldspawn, "floor");
    auto doorway = algorithm::getEntityByName(GlobalMapModule().getRoot(), "doorway");

    runCarve({ doorway, wall });

    EXPECT_TRUE(wall->getParent() == nullptr);
    EXPECT_TRUE(floor->getParent() != nullptr);
    expectDoorwayCut(worldspawn);
}

TEST_F(CsgTest, CarveOpeningWithOnlyEntitySelected)
{
    loadMap("carve_opening.map");

    auto worldspawn = GlobalMapModule().getWorldspawn();
    auto wall = algorithm::findFirstBrushWithMaterial(worldspawn, "wall");
    auto floor = algorithm::findFirstBrushWithMaterial(worldspawn, "floor");
    auto doorway = algorithm::getEntityByName(GlobalMapModule().getRoot(), "doorway");

    runCarve({ doorway });

    EXPECT_TRUE(wall->getParent() == nullptr);
    EXPECT_TRUE(floor->getParent() != nullptr);
    expectDoorwayCut(worldspawn);
}

TEST_F(CsgTest, CarveOpeningFollowsEntityRotation)
{
    loadMap("carve_opening.map");

    auto worldspawn = GlobalMapModule().getWorldspawn();
    auto wall = algorithm::findFirstBrushWithMaterial(worldspawn, "wall2");
    auto untouched = algorithm::findFirstBrushWithMaterial(worldspawn, "wall");
    auto doorway = algorithm::getEntityByName(GlobalMapModule().getRoot(), "doorway_rotated");

    runCarve({ doorway });

    EXPECT_TRUE(wall->getParent() == nullptr);
    EXPECT_TRUE(untouched->getParent() != nullptr);

    auto pieces = getBrushBoundsInside(worldspawn,
        AABB::createFromMinMax(Vector3(-97, 155, -1), Vector3(97, 165, 129)));

    EXPECT_EQ(pieces.size(), 3);
    EXPECT_FALSE(boundsContainPoint(pieces, Vector3(0, 160, 48)));
    EXPECT_TRUE(boundsContainPoint(pieces, Vector3(-29, 160, 48)));
    EXPECT_TRUE(boundsContainPoint(pieces, Vector3(29, 160, 48)));
    EXPECT_TRUE(boundsContainPoint(pieces, Vector3(0, 160, 97)));
    EXPECT_TRUE(boundsContainPoint(pieces, Vector3(-28.25, 160, 48)));
    EXPECT_TRUE(boundsContainPoint(pieces, Vector3(28.25, 160, 48)));

    auto materials = getBrushMaterialsInside(worldspawn,
        AABB::createFromMinMax(Vector3(-97, 155, -1), Vector3(97, 165, 129)));

    EXPECT_EQ(materials, std::set<std::string>({ "wall2" }));
}

TEST_F(CsgTest, PlaceOpeningOnWallFromCamera)
{
    loadMap("carve_opening.map");

    auto worldspawn = GlobalMapModule().getWorldspawn();
    auto doorway = algorithm::getEntityByName(GlobalMapModule().getRoot(), "doorway_rotated");

    GlobalGrid().setGridSize(GRID_M_02);
    GlobalSelectionSystem().setSelectedAll(false);
    Node_setSelected(doorway, true);

    GlobalCommandSystem().executeCommand("PlaceOpeningOnWall",
        cmd::ArgumentList{ Vector3(132, -32, 50), Vector3(1, 0, 0) });

    EXPECT_TRUE(math::isNear(doorway->worldAABB().getOrigin(), Vector3(128, -32, 0), 0.001));

    runCarve({ doorway });

    auto pieces = getBrushBoundsInside(worldspawn,
        AABB::createFromMinMax(Vector3(123, -97, -1), Vector3(133, 161, 129)));

    EXPECT_EQ(pieces.size(), 3);
    EXPECT_FALSE(boundsContainPoint(pieces, Vector3(128, -32, 48)));
    EXPECT_TRUE(boundsContainPoint(pieces, Vector3(128, -61, 48)));
    EXPECT_TRUE(boundsContainPoint(pieces, Vector3(128, -3, 48)));
    EXPECT_TRUE(boundsContainPoint(pieces, Vector3(128, -32, 97)));
}

TEST_F(CsgTest, PlaceOpeningOnWallFromTopView)
{
    loadMap("carve_opening.map");

    auto worldspawn = GlobalMapModule().getWorldspawn();
    auto doorway = algorithm::getEntityByName(GlobalMapModule().getRoot(), "doorway");

    GlobalGrid().setGridSize(GRID_M_02);
    GlobalSelectionSystem().setSelectedAll(false);
    Node_setSelected(doorway, true);

    GlobalCommandSystem().executeCommand("PlaceOpeningOnWall",
        cmd::ArgumentList{ Vector3(128, 64, 999), Vector3(0, 0, 0) });

    EXPECT_TRUE(math::isNear(doorway->worldAABB().getOrigin(), Vector3(128, 64, 48), 0.001));

    runCarve({ doorway });

    auto pieces = getBrushBoundsInside(worldspawn,
        AABB::createFromMinMax(Vector3(123, -97, -1), Vector3(133, 161, 129)));

    EXPECT_EQ(pieces.size(), 3);
    EXPECT_FALSE(boundsContainPoint(pieces, Vector3(128, 64, 48)));
    EXPECT_TRUE(boundsContainPoint(pieces, Vector3(128, 35, 48)));
    EXPECT_TRUE(boundsContainPoint(pieces, Vector3(128, 93, 48)));
}

TEST_F(CsgTest, CarveOpeningRecessesOnlyTheLinedDepth)
{
    loadMap("carve_overlap.map");

    auto worldspawn = GlobalMapModule().getWorldspawn();
    auto doorway = algorithm::getEntityByName(GlobalMapModule().getRoot(), "doorway");

    runCarve({ doorway });

    auto wallPieces = getBrushBoundsWithMaterial(worldspawn, "wall");
    auto roomPieces = getBrushBoundsWithMaterial(worldspawn, "roomwall");

    EXPECT_EQ(wallPieces.size(), 3);
    EXPECT_FALSE(boundsContainPoint(wallPieces, Vector3(128, 1.75, 48)));
    EXPECT_FALSE(boundsContainPoint(roomPieces, Vector3(126, 1.75, 48)));
    EXPECT_TRUE(boundsContainPoint(roomPieces, Vector3(100, 1.75, 48)));
    EXPECT_FALSE(boundsContainPoint(roomPieces, Vector3(100, 2.25, 48)));
    EXPECT_TRUE(boundsContainPoint(roomPieces, Vector3(100, 30, 97)));
    EXPECT_FALSE(boundsContainPoint(roomPieces, Vector3(126, 30, 96.25)));
}

}
