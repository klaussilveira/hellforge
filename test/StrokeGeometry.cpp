#include "gtest/gtest.h"

#include "xyview/tools/StrokeGeometry.h"

namespace test
{

using namespace ui::strokegeometry;

TEST(StrokeGeometryTest, SimplifyDropsPointsOnStraightLine)
{
    std::vector<Vector3> stroke =
    {
        { 0, 0, 0 }, { 10, 0, 0 }, { 20, 0, 0 }, { 30, 0, 0 }, { 40, 0, 0 }
    };

    auto result = simplify(stroke, 1.0);

    ASSERT_EQ(result.size(), 2);
    EXPECT_EQ(result.front(), Vector3(0, 0, 0));
    EXPECT_EQ(result.back(), Vector3(40, 0, 0));
}

TEST(StrokeGeometryTest, SimplifyKeepsPointsExceedingTolerance)
{
    std::vector<Vector3> stroke =
    {
        { 0, 0, 0 }, { 10, 5, 0 }, { 20, 0, 0 }
    };

    auto result = simplify(stroke, 1.0);

    ASSERT_EQ(result.size(), 3);
    EXPECT_EQ(result[1], Vector3(10, 5, 0));
}

TEST(StrokeGeometryTest, SimplifyDropsPointWithinTolerance)
{
    std::vector<Vector3> stroke =
    {
        { 0, 0, 0 }, { 10, 5, 0 }, { 20, 0, 0 }
    };

    auto result = simplify(stroke, 6.0);

    ASSERT_EQ(result.size(), 2);
    EXPECT_EQ(result.front(), Vector3(0, 0, 0));
    EXPECT_EQ(result.back(), Vector3(20, 0, 0));
}

TEST(StrokeGeometryTest, SmoothMovesInteriorPointsAndKeepsEnds)
{
    std::vector<Vector3> stroke =
    {
        { 0, 0, 0 }, { 10, 8, 0 }, { 20, 0, 0 }
    };

    auto result = smooth(stroke, 1);

    ASSERT_EQ(result.size(), 3);
    EXPECT_EQ(result.front(), Vector3(0, 0, 0));
    EXPECT_EQ(result.back(), Vector3(20, 0, 0));
    EXPECT_NEAR(result[1].x(), 10, 1e-9);
    EXPECT_NEAR(result[1].y(), 4, 1e-9);
}

TEST(StrokeGeometryTest, SmoothLeavesShortStrokesUntouched)
{
    std::vector<Vector3> stroke = { { 0, 0, 0 }, { 10, 8, 0 } };

    auto result = smooth(stroke, 4);

    ASSERT_EQ(result.size(), 2);
    EXPECT_EQ(result[1], Vector3(10, 8, 0));
}

TEST(StrokeGeometryTest, ProcessStrokeWithoutSmoothingKeepsEveryPoint)
{
    std::vector<Vector3> stroke =
    {
        { 0, 0, 0 }, { 10, 1, 0 }, { 20, 0, 0 }, { 30, 1, 0 }
    };

    auto result = processStroke(stroke, false, 8.0);

    EXPECT_EQ(result, stroke);
}

TEST(StrokeGeometryTest, ProcessStrokeCollapsesJitterWithinToleranceToAStraightLine)
{
    std::vector<Vector3> stroke;

    for (int i = 0; i <= 40; ++i)
    {
        stroke.push_back(Vector3(i * 4, (i % 2) == 0 ? 1 : -1, 0));
    }

    auto result = processStroke(stroke, true, 8.0);

    ASSERT_EQ(result.size(), 3);
    EXPECT_EQ(result[0], Vector3(0, 1, 0));
    EXPECT_EQ(result[1], Vector3(80, 1, 0));
    EXPECT_EQ(result[2], Vector3(160, 1, 0));
}

TEST(StrokeGeometryTest, ProcessStrokeKeepsBendsLargerThanTolerance)
{
    std::vector<Vector3> stroke =
    {
        { 0, 0, 0 }, { 20, 20, 0 }, { 40, 40, 0 }, { 60, 20, 0 }, { 80, 0, 0 }
    };

    auto result = processStroke(stroke, true, 8.0);

    ASSERT_EQ(result.size(), 3);
    EXPECT_EQ(result.front(), Vector3(0, 0, 0));
    EXPECT_EQ(result.back(), Vector3(80, 0, 0));
    EXPECT_NEAR(result[1].x(), 40, 1e-9);
    EXPECT_NEAR(result[1].y(), 25, 1e-9);
}

TEST(StrokeGeometryTest, ProcessStrokeInsertsMidPointForStraightLine)
{
    std::vector<Vector3> stroke = { { 0, 0, 0 }, { 100, 50, 0 } };

    auto result = processStroke(stroke, false, 8.0);

    ASSERT_EQ(result.size(), 3);
    EXPECT_EQ(result[0], Vector3(0, 0, 0));
    EXPECT_EQ(result[1], Vector3(50, 25, 0));
    EXPECT_EQ(result[2], Vector3(100, 50, 0));
}

}
