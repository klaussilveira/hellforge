#include "RadiantTest.h"

#include "ibrush.h"
#include "imap.h"
#include "scenelib.h"
#include "math/Vector3.h"
#include "math/AABB.h"
#include "algorithm/Scene.h"

#include "ui/building/BuildingGeometry.h"

namespace test
{

using BuildingGeneratorTest = RadiantTest;

std::vector<AABB> collectBrushBounds(const scene::INodePtr& parent)
{
    std::vector<AABB> result;
    parent->foreachNode([&](const scene::INodePtr& child)
    {
        if (Node_getIBrush(child))
            result.push_back(child->worldAABB());
        return true;
    });
    return result;
}

std::size_t countBrushChildren(const scene::INodePtr& parent)
{
    std::size_t count = 0;
    parent->foreachNode([&](const scene::INodePtr& child)
    {
        if (Node_getIBrush(child))
            ++count;
        return true;
    });
    return count;
}

// computeAutoWindowCount tests
TEST_F(BuildingGeneratorTest, AutoWindowCountZeroForSmallWall)
{
    EXPECT_EQ(building::computeAutoWindowCount(30, 48), 0);
}

TEST_F(BuildingGeneratorTest, AutoWindowCountOneForMinimalWall)
{
    EXPECT_EQ(building::computeAutoWindowCount(80, 48), 1);
}

TEST_F(BuildingGeneratorTest, AutoWindowCountScalesWithLength)
{
    int count = building::computeAutoWindowCount(500, 48);
    EXPECT_GE(count, 2);
}

TEST_F(BuildingGeneratorTest, AutoWindowCountZeroForZeroWidth)
{
    EXPECT_EQ(building::computeAutoWindowCount(256, 0), 0);
}

TEST_F(BuildingGeneratorTest, AutoWindowCountZeroForNegativeLength)
{
    EXPECT_EQ(building::computeAutoWindowCount(-10, 48), 0);
}

// createBoxBrush tests
TEST_F(BuildingGeneratorTest, CreateBoxBrushSixFaces)
{
    auto worldspawn = GlobalMapModule().findOrInsertWorldspawn();
    auto node = building::createBoxBrush(
        Vector3(0, 0, 0), Vector3(64, 64, 64), "_default", worldspawn);
    ASSERT_TRUE(node != nullptr);

    auto* brush = Node_getIBrush(node);
    ASSERT_TRUE(brush != nullptr);
    EXPECT_EQ(brush->getNumFaces(), 6);
}

TEST_F(BuildingGeneratorTest, CreateBoxBrushCorrectBounds)
{
    auto worldspawn = GlobalMapModule().findOrInsertWorldspawn();
    auto node = building::createBoxBrush(
        Vector3(10, 20, 30), Vector3(50, 60, 70), "_default", worldspawn);
    ASSERT_TRUE(node != nullptr);

    AABB bounds = node->worldAABB();
    EXPECT_NEAR(bounds.getOrigin().x(), 30, 0.5);
    EXPECT_NEAR(bounds.getOrigin().y(), 40, 0.5);
    EXPECT_NEAR(bounds.getOrigin().z(), 50, 0.5);
    EXPECT_NEAR(bounds.getExtents().x(), 20, 0.5);
    EXPECT_NEAR(bounds.getExtents().y(), 20, 0.5);
    EXPECT_NEAR(bounds.getExtents().z(), 20, 0.5);
}

TEST_F(BuildingGeneratorTest, CreateBoxBrushRejectsDegenerate)
{
    auto worldspawn = GlobalMapModule().findOrInsertWorldspawn();
    auto node = building::createBoxBrush(
        Vector3(0, 0, 0), Vector3(0.5, 64, 64), "_default", worldspawn);
    EXPECT_TRUE(node == nullptr);
}

// generateBuilding basic structure
TEST_F(BuildingGeneratorTest, FlatRoofNoWindowsBrushCount)
{
    auto worldspawn = GlobalMapModule().findOrInsertWorldspawn();

    building::BuildingParams params;
    params.floorCount = 2;
    params.floorHeight = 128;
    params.wallThickness = 8;
    params.trimHeight = 8;
    params.windowsPerFloor = -1;
    params.roofType = 0;

    building::generateBuilding(
        Vector3(0, 0, 0), Vector3(256, 256, 256), params, worldspawn);

    // Per floor: 1 trim slab + 4 walls = 5
    // 2 floors = 10
    // + 1 top trim slab = 11
    // Flat roof adds nothing
    EXPECT_EQ(countBrushChildren(worldspawn), 11);
}

TEST_F(BuildingGeneratorTest, SingleFloorWallExtents)
{
    auto worldspawn = GlobalMapModule().findOrInsertWorldspawn();

    building::BuildingParams params;
    params.floorCount = 1;
    params.floorHeight = 128;
    params.wallThickness = 8;
    params.trimHeight = 8;
    params.windowsPerFloor = -1;
    params.roofType = 0;

    building::generateBuilding(
        Vector3(0, 0, 0), Vector3(256, 256, 128), params, worldspawn);

    auto bounds = collectBrushBounds(worldspawn);
    // 1 trim + 4 walls + 1 top trim = 6
    ASSERT_EQ(bounds.size(), 6);

    // Check that trim slab covers full XY footprint
    AABB trim = bounds[0];
    EXPECT_NEAR(trim.getOrigin().x() - trim.getExtents().x(), 0, 0.5);
    EXPECT_NEAR(trim.getOrigin().x() + trim.getExtents().x(), 256, 0.5);
    EXPECT_NEAR(trim.getOrigin().y() - trim.getExtents().y(), 0, 0.5);
    EXPECT_NEAR(trim.getOrigin().y() + trim.getExtents().y(), 256, 0.5);
}

TEST_F(BuildingGeneratorTest, AutoFloorHeightDividesEvenly)
{
    auto worldspawn = GlobalMapModule().findOrInsertWorldspawn();

    building::BuildingParams params;
    params.floorCount = 3;
    params.floorHeight = 0;
    params.wallThickness = 8;
    params.trimHeight = 8;
    params.windowsPerFloor = -1;
    params.roofType = 0;

    building::generateBuilding(
        Vector3(0, 0, 0), Vector3(256, 256, 384), params, worldspawn);

    auto bounds = collectBrushBounds(worldspawn);
    // 3 floors * (1 trim + 4 walls) + 1 top trim = 16
    ASSERT_EQ(bounds.size(), 16);

    // First floor trim: z from 0 to 8
    EXPECT_NEAR(bounds[0].getOrigin().z() - bounds[0].getExtents().z(), 0, 0.5);
    EXPECT_NEAR(bounds[0].getOrigin().z() + bounds[0].getExtents().z(), 8, 0.5);

    // Second floor trim: z from 128 to 136
    EXPECT_NEAR(bounds[5].getOrigin().z() - bounds[5].getExtents().z(), 128, 0.5);
    EXPECT_NEAR(bounds[5].getOrigin().z() + bounds[5].getExtents().z(), 136, 0.5);

    // Third floor trim: z from 256 to 264
    EXPECT_NEAR(bounds[10].getOrigin().z() - bounds[10].getExtents().z(), 256, 0.5);
    EXPECT_NEAR(bounds[10].getOrigin().z() + bounds[10].getExtents().z(), 264, 0.5);
}

// Window generation
TEST_F(BuildingGeneratorTest, WallWithWindowsCreatesSillAndHeader)
{
    auto worldspawn = GlobalMapModule().findOrInsertWorldspawn();

    building::generateWallWithWindows(
        Vector3(0, 0, 0), Vector3(8, 256, 120),
        1, 1,
        48, 56, 32,
        "_default", worldspawn);

    auto bounds = collectBrushBounds(worldspawn);
    // 1 window: left col + sill + header + right col = 4
    // (assuming wall is wide enough for columns on both sides)
    EXPECT_GE(bounds.size(), 3u);
}

TEST_F(BuildingGeneratorTest, WallWithNoWindowsCreatesSolidBrush)
{
    auto worldspawn = GlobalMapModule().findOrInsertWorldspawn();

    building::generateWallWithWindows(
        Vector3(0, 0, 0), Vector3(8, 256, 120),
        1, 0,
        48, 56, 32,
        "_default", worldspawn);

    EXPECT_EQ(countBrushChildren(worldspawn), 1);
}

TEST_F(BuildingGeneratorTest, WindowsTooWideForWallFallsBackToSolid)
{
    auto worldspawn = GlobalMapModule().findOrInsertWorldspawn();

    building::generateWallWithWindows(
        Vector3(0, 0, 0), Vector3(8, 64, 120),
        1, 3,
        48, 56, 32,
        "_default", worldspawn);

    // 3 windows * 48 = 144 > 64 wall length, should fall back to solid
    EXPECT_EQ(countBrushChildren(worldspawn), 1);
}

// Roof types
TEST_F(BuildingGeneratorTest, FlatWithBorderAdds4Brushes)
{
    auto worldspawn = GlobalMapModule().findOrInsertWorldspawn();

    building::BuildingParams params;
    params.floorCount = 1;
    params.floorHeight = 128;
    params.wallThickness = 8;
    params.trimHeight = 8;
    params.windowsPerFloor = -1;
    params.roofType = 1;
    params.roofBorderHeight = 16;

    building::generateBuilding(
        Vector3(0, 0, 0), Vector3(256, 256, 128), params, worldspawn);

    // 1 trim + 4 walls + 1 top trim + 4 border = 10
    EXPECT_EQ(countBrushChildren(worldspawn), 10);
}

TEST_F(BuildingGeneratorTest, SlantedRoofAdds1Brush)
{
    auto worldspawn = GlobalMapModule().findOrInsertWorldspawn();

    building::BuildingParams params;
    params.floorCount = 1;
    params.floorHeight = 128;
    params.wallThickness = 8;
    params.trimHeight = 8;
    params.windowsPerFloor = -1;
    params.roofType = 2;
    params.roofHeight = 64;

    building::generateBuilding(
        Vector3(0, 0, 0), Vector3(256, 256, 128), params, worldspawn);

    // 1 trim + 4 walls + 1 top trim + 1 roof wedge = 7
    EXPECT_EQ(countBrushChildren(worldspawn), 7);
}

TEST_F(BuildingGeneratorTest, AFrameRoofAdds2Brushes)
{
    auto worldspawn = GlobalMapModule().findOrInsertWorldspawn();

    building::BuildingParams params;
    params.floorCount = 1;
    params.floorHeight = 128;
    params.wallThickness = 8;
    params.trimHeight = 8;
    params.windowsPerFloor = -1;
    params.roofType = 3;
    params.roofHeight = 64;

    building::generateBuilding(
        Vector3(0, 0, 0), Vector3(256, 256, 128), params, worldspawn);

    // 1 trim + 4 walls + 1 top trim + 2 roof wedges = 8
    EXPECT_EQ(countBrushChildren(worldspawn), 8);
}

TEST_F(BuildingGeneratorTest, SlantedRoofPeakHeight)
{
    auto worldspawn = GlobalMapModule().findOrInsertWorldspawn();

    building::BuildingParams params;
    params.floorCount = 1;
    params.floorHeight = 128;
    params.wallThickness = 8;
    params.trimHeight = 8;
    params.windowsPerFloor = -1;
    params.roofType = 2;
    params.roofHeight = 64;

    building::generateBuilding(
        Vector3(0, 0, 0), Vector3(256, 256, 128), params, worldspawn);

    auto bounds = collectBrushBounds(worldspawn);
    AABB roofBounds = bounds.back();
    double roofTop = roofBounds.getOrigin().z() + roofBounds.getExtents().z();
    // Roof base = 128 + 8 = 136, peak = 136 + 64 = 200
    EXPECT_NEAR(roofTop, 200, 1.0);
}

// Corner columns
TEST_F(BuildingGeneratorTest, CornerColumnsAdd4Brushes)
{
    auto worldspawn = GlobalMapModule().findOrInsertWorldspawn();

    building::BuildingParams params;
    params.floorCount = 1;
    params.floorHeight = 128;
    params.wallThickness = 8;
    params.trimHeight = 8;
    params.windowsPerFloor = -1;
    params.roofType = 0;
    params.cornerColumns = true;

    building::generateBuilding(
        Vector3(0, 0, 0), Vector3(256, 256, 128), params, worldspawn);

    // Without corners: 1 trim + 4 walls + 1 top trim = 6
    // With corners: + 4 columns = 10
    EXPECT_EQ(countBrushChildren(worldspawn), 10);
}

TEST_F(BuildingGeneratorTest, CornerColumnsSpanFullHeight)
{
    auto worldspawn = GlobalMapModule().findOrInsertWorldspawn();

    building::BuildingParams params;
    params.floorCount = 3;
    params.floorHeight = 128;
    params.wallThickness = 8;
    params.trimHeight = 8;
    params.windowsPerFloor = -1;
    params.roofType = 0;
    params.cornerColumns = true;

    building::generateBuilding(
        Vector3(0, 0, 0), Vector3(256, 256, 384), params, worldspawn);

    auto bounds = collectBrushBounds(worldspawn);

    // Corner columns are the first 4 brushes
    double expectedTop = 384 + 8; // floorCount * floorHeight + trimHeight
    for (int i = 0; i < 4; ++i)
    {
        double colBot = bounds[i].getOrigin().z() - bounds[i].getExtents().z();
        double colTop = bounds[i].getOrigin().z() + bounds[i].getExtents().z();
        EXPECT_NEAR(colBot, 0, 0.5) << "Column " << i << " bottom";
        EXPECT_NEAR(colTop, expectedTop, 0.5) << "Column " << i << " top";
    }
}

TEST_F(BuildingGeneratorTest, CornerExtrudeExtendsOutward)
{
    auto worldspawn = GlobalMapModule().findOrInsertWorldspawn();

    building::BuildingParams params;
    params.floorCount = 1;
    params.floorHeight = 128;
    params.wallThickness = 8;
    params.trimHeight = 8;
    params.windowsPerFloor = -1;
    params.roofType = 0;
    params.cornerColumns = true;
    params.cornerExtrude = 4;

    building::generateBuilding(
        Vector3(0, 0, 0), Vector3(256, 256, 128), params, worldspawn);

    auto bounds = collectBrushBounds(worldspawn);

    // With extrude=4, corners should extend beyond 0..256 footprint
    bool hasNegativeX = false;
    bool hasBeyondMaxX = false;
    bool hasNegativeY = false;
    bool hasBeyondMaxY = false;

    for (int i = 0; i < 4; ++i)
    {
        double minX = bounds[i].getOrigin().x() - bounds[i].getExtents().x();
        double maxX = bounds[i].getOrigin().x() + bounds[i].getExtents().x();
        double minY = bounds[i].getOrigin().y() - bounds[i].getExtents().y();
        double maxY = bounds[i].getOrigin().y() + bounds[i].getExtents().y();

        if (minX < -0.5) hasNegativeX = true;
        if (maxX > 256.5) hasBeyondMaxX = true;
        if (minY < -0.5) hasNegativeY = true;
        if (maxY > 256.5) hasBeyondMaxY = true;
    }

    EXPECT_TRUE(hasNegativeX);
    EXPECT_TRUE(hasBeyondMaxX);
    EXPECT_TRUE(hasNegativeY);
    EXPECT_TRUE(hasBeyondMaxY);
}

// Multi-floor with windows
TEST_F(BuildingGeneratorTest, FixedWindowCountGeneratesWindowOpenings)
{
    auto worldspawn = GlobalMapModule().findOrInsertWorldspawn();

    building::BuildingParams params;
    params.floorCount = 1;
    params.floorHeight = 128;
    params.wallThickness = 8;
    params.trimHeight = 8;
    params.windowsPerFloor = 2;
    params.windowWidth = 48;
    params.windowHeight = 56;
    params.sillHeight = 32;
    params.roofType = 0;

    building::generateBuilding(
        Vector3(0, 0, 0), Vector3(256, 256, 128), params, worldspawn);

    std::size_t count = countBrushChildren(worldspawn);
    // More brushes than the no-windows case (6) because walls are subdivided
    EXPECT_GT(count, 6u);
}

} // namespace test
