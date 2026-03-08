#include "RadiantTest.h"

#include "ibrush.h"
#include "imap.h"
#include "iselection.h"
#include "scenelib.h"
#include "math/Vector3.h"
#include "math/AABB.h"
#include "algorithm/Scene.h"
#include "algorithm/Primitives.h"

#include "ui/stairs/StairsGeometry.h"

namespace test
{

using StairsGeneratorTest = RadiantTest;

// Helper: collect child brush bounds into a vector
std::vector<AABB> collectChildBounds(const scene::INodePtr& parent)
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

// -- dirFromDegrees tests --

TEST_F(StairsGeneratorTest, DirFromDegreesEast)
{
    auto d = stairs::dirFromDegrees(0);
    EXPECT_NEAR(d.dx, 1.0, 1e-9);
    EXPECT_NEAR(d.dy, 0.0, 1e-9);
    // Left perpendicular of East is North
    EXPECT_NEAR(d.px, 0.0, 1e-9);
    EXPECT_NEAR(d.py, 1.0, 1e-9);
}

TEST_F(StairsGeneratorTest, DirFromDegreesNorth)
{
    auto d = stairs::dirFromDegrees(90);
    EXPECT_NEAR(d.dx, 0.0, 1e-9);
    EXPECT_NEAR(d.dy, 1.0, 1e-9);
    // Left perpendicular of North is West
    EXPECT_NEAR(d.px, -1.0, 1e-9);
    EXPECT_NEAR(d.py, 0.0, 1e-9);
}

TEST_F(StairsGeneratorTest, DirFromDegreesWest)
{
    auto d = stairs::dirFromDegrees(180);
    EXPECT_NEAR(d.dx, -1.0, 1e-9);
    EXPECT_NEAR(d.dy, 0.0, 1e-9);
    // Left perpendicular of West is South
    EXPECT_NEAR(d.px, 0.0, 1e-9);
    EXPECT_NEAR(d.py, -1.0, 1e-9);
}

TEST_F(StairsGeneratorTest, DirFromDegreesSouth)
{
    auto d = stairs::dirFromDegrees(270);
    EXPECT_NEAR(d.dx, 0.0, 1e-9);
    EXPECT_NEAR(d.dy, -1.0, 1e-9);
    // Left perpendicular of South is East
    EXPECT_NEAR(d.px, 1.0, 1e-9);
    EXPECT_NEAR(d.py, 0.0, 1e-9);
}

// -- Straight stairs tests --

TEST_F(StairsGeneratorTest, StraightStairsCreatesCorrectCount)
{
    auto worldspawn = GlobalMapModule().findOrInsertWorldspawn();
    auto before = algorithm::getChildCount(worldspawn);

    stairs::generateStraightStairs(
        Vector3(0, 0, 0), 5, 16, 16, 64, 0, false, "_default", worldspawn);

    EXPECT_EQ(algorithm::getChildCount(worldspawn) - before, 5);
}

TEST_F(StairsGeneratorTest, StraightStairsSlabHeights)
{
    auto worldspawn = GlobalMapModule().findOrInsertWorldspawn();
    stairs::generateStraightStairs(
        Vector3(0, 0, 0), 4, 16, 16, 64, 0, false, "_default", worldspawn);

    auto bounds = collectChildBounds(worldspawn);
    ASSERT_EQ(bounds.size(), 4);

    // In slab mode each step is exactly stepH tall
    for (size_t i = 0; i < bounds.size(); ++i)
    {
        double expectedTop = (i + 1) * 16.0;
        double expectedBot = i * 16.0;
        EXPECT_NEAR(bounds[i].getOrigin().z() + bounds[i].getExtents().z(), expectedTop, 0.1)
            << "Step " << i << " top";
        EXPECT_NEAR(bounds[i].getOrigin().z() - bounds[i].getExtents().z(), expectedBot, 0.1)
            << "Step " << i << " bottom";
    }
}

TEST_F(StairsGeneratorTest, StraightStairsSolidMode)
{
    auto worldspawn = GlobalMapModule().findOrInsertWorldspawn();
    stairs::generateStraightStairs(
        Vector3(0, 0, 0), 4, 16, 16, 64, 0, true, "_default", worldspawn);

    auto bounds = collectChildBounds(worldspawn);
    ASSERT_EQ(bounds.size(), 4);

    // In solid mode all steps start at z=0
    for (size_t i = 0; i < bounds.size(); ++i)
    {
        double expectedBot = 0.0;
        double expectedTop = (i + 1) * 16.0;
        EXPECT_NEAR(bounds[i].getOrigin().z() - bounds[i].getExtents().z(), expectedBot, 0.1)
            << "Step " << i << " bottom should be 0";
        EXPECT_NEAR(bounds[i].getOrigin().z() + bounds[i].getExtents().z(), expectedTop, 0.1)
            << "Step " << i << " top";
    }
}

TEST_F(StairsGeneratorTest, StraightStairsEastDirection)
{
    auto worldspawn = GlobalMapModule().findOrInsertWorldspawn();
    stairs::generateStraightStairs(
        Vector3(0, 0, 0), 3, 16, 16, 64, 0, false, "_default", worldspawn);

    auto bounds = collectChildBounds(worldspawn);
    ASSERT_EQ(bounds.size(), 3);

    // East (0 deg): steps advance along +X
    for (size_t i = 0; i < bounds.size(); ++i)
    {
        double expectedXMin = i * 16.0;
        double expectedXMax = (i + 1) * 16.0;
        EXPECT_NEAR(bounds[i].getOrigin().x() - bounds[i].getExtents().x(), expectedXMin, 0.1)
            << "Step " << i << " xmin";
        EXPECT_NEAR(bounds[i].getOrigin().x() + bounds[i].getExtents().x(), expectedXMax, 0.1)
            << "Step " << i << " xmax";
    }
}

TEST_F(StairsGeneratorTest, StraightStairsNorthDirection)
{
    auto worldspawn = GlobalMapModule().findOrInsertWorldspawn();
    stairs::generateStraightStairs(
        Vector3(0, 0, 0), 3, 16, 16, 64, 90, false, "_default", worldspawn);

    auto bounds = collectChildBounds(worldspawn);
    ASSERT_EQ(bounds.size(), 3);

    // North (90 deg): steps advance along +Y
    for (size_t i = 0; i < bounds.size(); ++i)
    {
        double expectedYMin = i * 16.0;
        double expectedYMax = (i + 1) * 16.0;
        EXPECT_NEAR(bounds[i].getOrigin().y() - bounds[i].getExtents().y(), expectedYMin, 0.1)
            << "Step " << i << " ymin";
        EXPECT_NEAR(bounds[i].getOrigin().y() + bounds[i].getExtents().y(), expectedYMax, 0.1)
            << "Step " << i << " ymax";
    }
}

TEST_F(StairsGeneratorTest, StraightStairsWidth)
{
    auto worldspawn = GlobalMapModule().findOrInsertWorldspawn();
    stairs::generateStraightStairs(
        Vector3(0, 0, 0), 2, 16, 16, 64, 0, false, "_default", worldspawn);

    auto bounds = collectChildBounds(worldspawn);
    ASSERT_EQ(bounds.size(), 2);

    // East direction: width is along Y axis (left perpendicular = +Y)
    for (size_t i = 0; i < bounds.size(); ++i)
    {
        double yExtent = bounds[i].getExtents().y() * 2;
        EXPECT_NEAR(yExtent, 64.0, 0.1) << "Step " << i << " width";
    }
}

// -- L-shape stairs tests --

TEST_F(StairsGeneratorTest, LShapeCreatesCorrectBrushCount)
{
    auto worldspawn = GlobalMapModule().findOrInsertWorldspawn();
    auto before = algorithm::getChildCount(worldspawn);

    // 8 steps, turn at step 4 -> 4 first run + 1 landing + 3 second run = 8
    stairs::generateLShapeStairs(
        Vector3(0, 0, 0), 8, 16, 16, 64, 0, false, 4, 0, "_default", worldspawn);

    EXPECT_EQ(algorithm::getChildCount(worldspawn) - before, 8);
}

TEST_F(StairsGeneratorTest, LShapeLeftTurnSecondRunGoesNorth)
{
    auto worldspawn = GlobalMapModule().findOrInsertWorldspawn();

    // East direction, left turn at step 3 of 6 total
    // Turn left from East = North (+Y direction)
    stairs::generateLShapeStairs(
        Vector3(0, 0, 0), 6, 16, 16, 64, 0, false, 3, 0, "_default", worldspawn);

    auto bounds = collectChildBounds(worldspawn);
    // 3 first run + 1 landing + 2 second run = 6
    ASSERT_EQ(bounds.size(), 6);

    // Second run brushes (indices 4, 5) should have positive Y values
    // beyond the landing, confirming they go North
    double landingMaxY = bounds[3].getOrigin().y() + bounds[3].getExtents().y();
    for (size_t i = 4; i < bounds.size(); ++i)
    {
        double brushMinY = bounds[i].getOrigin().y() - bounds[i].getExtents().y();
        EXPECT_GE(brushMinY, landingMaxY - 0.1)
            << "Second run brush " << (i - 4) << " should be north of landing";
    }
}

TEST_F(StairsGeneratorTest, LShapeRightTurnSecondRunGoesSouth)
{
    auto worldspawn = GlobalMapModule().findOrInsertWorldspawn();

    // East direction, right turn at step 3 of 6 total
    // Turn right from East = South (-Y direction)
    stairs::generateLShapeStairs(
        Vector3(0, 0, 0), 6, 16, 16, 64, 0, false, 3, 1, "_default", worldspawn);

    auto bounds = collectChildBounds(worldspawn);
    ASSERT_EQ(bounds.size(), 6);

    // Second run brushes should have negative Y values (south of origin)
    double landingMinY = bounds[3].getOrigin().y() - bounds[3].getExtents().y();
    for (size_t i = 4; i < bounds.size(); ++i)
    {
        double brushMaxY = bounds[i].getOrigin().y() + bounds[i].getExtents().y();
        EXPECT_LE(brushMaxY, landingMinY + 0.1)
            << "Second run brush " << (i - 4) << " should be south of landing";
    }
}

// -- U-shape stairs tests --

TEST_F(StairsGeneratorTest, UShapeCreatesCorrectBrushCount)
{
    auto worldspawn = GlobalMapModule().findOrInsertWorldspawn();
    auto before = algorithm::getChildCount(worldspawn);

    // 8 steps, turn at step 4 -> 4 first run + 1 landing + 3 second run = 8
    stairs::generateUShapeStairs(
        Vector3(0, 0, 0), 8, 16, 16, 64, 0, false, 4, 0, 32, "_default", worldspawn);

    EXPECT_EQ(algorithm::getChildCount(worldspawn) - before, 8);
}

TEST_F(StairsGeneratorTest, UShapeSecondRunReturnsAlongside)
{
    auto worldspawn = GlobalMapModule().findOrInsertWorldspawn();

    // East direction, left turn, 8 steps, turn at 4
    stairs::generateUShapeStairs(
        Vector3(0, 0, 0), 8, 16, 16, 64, 0, false, 4, 0, 32, "_default", worldspawn);

    auto bounds = collectChildBounds(worldspawn);
    ASSERT_EQ(bounds.size(), 8);

    // First run goes East (+X). Second run (180 deg turn) goes West (-X).
    // Second run brushes (indices 5, 6, 7) should have X values within
    // the X range of the first run, not extending further East.
    double firstRunMaxX = 0;
    for (size_t i = 0; i < 4; ++i)
        firstRunMaxX = std::max(firstRunMaxX,
            bounds[i].getOrigin().x() + bounds[i].getExtents().x());

    // Landing extends further but second run steps should come back
    for (size_t i = 5; i < bounds.size(); ++i)
    {
        double brushMaxX = bounds[i].getOrigin().x() + bounds[i].getExtents().x();
        EXPECT_LE(brushMaxX, firstRunMaxX + 0.1)
            << "Second run brush " << (i - 5) << " should not extend beyond first run";
    }
}

TEST_F(StairsGeneratorTest, UShapeSecondRunOffsetSideways)
{
    auto worldspawn = GlobalMapModule().findOrInsertWorldspawn();

    // East direction, left turn (turnDir=0), width=64
    // Left perpendicular of East is North (+Y)
    // Second run should be offset 2*width=128 in +Y from first run
    stairs::generateUShapeStairs(
        Vector3(0, 0, 0), 8, 16, 16, 64, 0, false, 4, 0, 32, "_default", worldspawn);

    auto bounds = collectChildBounds(worldspawn);
    ASSERT_EQ(bounds.size(), 8);

    // First run step 0: Y range = [0, 64]
    double firstRunMinY = bounds[0].getOrigin().y() - bounds[0].getExtents().y();

    // Second run steps should be at Y range starting at 2*width=128 from first run origin
    double secondRunMinY = bounds[5].getOrigin().y() - bounds[5].getExtents().y();
    EXPECT_NEAR(secondRunMinY, firstRunMinY + 64.0, 0.1)
        << "Second run should be offset by one width gap from first run";
}

// -- Spiral stairs tests --

TEST_F(StairsGeneratorTest, SpiralCreatesCorrectCount)
{
    auto worldspawn = GlobalMapModule().findOrInsertWorldspawn();
    auto before = algorithm::getChildCount(worldspawn);

    stairs::generateSpiralStairs(
        Vector3(0, 0, 0), 12, 16, 0, false, 32, 96, 360, "_default", worldspawn);

    EXPECT_EQ(algorithm::getChildCount(worldspawn) - before, 12);
}

TEST_F(StairsGeneratorTest, SpiralBrushesHaveSixFaces)
{
    auto worldspawn = GlobalMapModule().findOrInsertWorldspawn();

    stairs::generateSpiralStairs(
        Vector3(0, 0, 0), 4, 16, 0, false, 32, 96, 90, "_default", worldspawn);

    worldspawn->foreachNode([](const scene::INodePtr& child)
    {
        auto* brush = Node_getIBrush(child);
        if (brush)
            EXPECT_EQ(brush->getNumFaces(), 6) << "Each spiral step should have 6 faces";
        return true;
    });
}

TEST_F(StairsGeneratorTest, SpiralStepHeights)
{
    auto worldspawn = GlobalMapModule().findOrInsertWorldspawn();

    stairs::generateSpiralStairs(
        Vector3(0, 0, 0), 4, 16, 0, false, 32, 96, 360, "_default", worldspawn);

    auto bounds = collectChildBounds(worldspawn);
    ASSERT_EQ(bounds.size(), 4);

    for (size_t i = 0; i < bounds.size(); ++i)
    {
        double expectedTop = (i + 1) * 16.0;
        EXPECT_NEAR(bounds[i].getOrigin().z() + bounds[i].getExtents().z(), expectedTop, 0.5)
            << "Spiral step " << i << " top";
    }
}

TEST_F(StairsGeneratorTest, SpiralBoundsWithinOuterRadius)
{
    auto worldspawn = GlobalMapModule().findOrInsertWorldspawn();
    double outerR = 96;
    int stepCount = 12;
    double totalAngle = 360;

    stairs::generateSpiralStairs(
        Vector3(0, 0, 0), stepCount, 16, 0, false, 32, outerR, totalAngle, "_default", worldspawn);

    // Chord approximation causes corners to extend beyond outerR by outerR/cos(halfAngle) - outerR
    double halfAngleRad = (totalAngle / stepCount / 2.0) * stairs::DEG2RAD;
    double chordOvershoot = outerR / std::cos(halfAngleRad) - outerR;

    auto bounds = collectChildBounds(worldspawn);
    for (size_t i = 0; i < bounds.size(); ++i)
    {
        double maxXDist = std::abs(bounds[i].getOrigin().x()) + bounds[i].getExtents().x();
        double maxYDist = std::abs(bounds[i].getOrigin().y()) + bounds[i].getExtents().y();
        EXPECT_LE(maxXDist, outerR + chordOvershoot + 1.0) << "Step " << i << " X within outer radius";
        EXPECT_LE(maxYDist, outerR + chordOvershoot + 1.0) << "Step " << i << " Y within outer radius";
    }
}

} // namespace test
