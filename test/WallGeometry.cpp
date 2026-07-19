#include "gtest/gtest.h"

#include <cmath>

#include "xyview/tools/WallGeometry.h"

namespace test
{

using namespace ui::wallgeometry;

TEST(WallGeometryTest, SnapSegmentEndAxis)
{
    Vector2 end = snapSegmentEnd(Vector2(0, 0), Vector2(99, 5), 8.0);

    EXPECT_NEAR(end.x(), 96, 1e-9);
    EXPECT_NEAR(end.y(), 0, 1e-9);
}

TEST(WallGeometryTest, SnapSegmentEndDiagonal)
{
    Vector2 end = snapSegmentEnd(Vector2(0, 0), Vector2(70, 66), 8.0);

    EXPECT_NEAR(end.x(), 64, 1e-9);
    EXPECT_NEAR(end.y(), 64, 1e-9);
}

TEST(WallGeometryTest, SnapSegmentEndTinyDragStaysAtAnchor)
{
    Vector2 end = snapSegmentEnd(Vector2(0, 0), Vector2(3, 1), 8.0);

    EXPECT_EQ(end.x(), 0);
    EXPECT_EQ(end.y(), 0);
}

TEST(WallGeometryTest, SnapSegmentEndFromOffsetAnchor)
{
    Vector2 end = snapSegmentEnd(Vector2(64, 64), Vector2(-40, 64), 8.0);

    EXPECT_NEAR(end.x(), -40, 1e-9);
    EXPECT_NEAR(end.y(), 64, 1e-9);
}

TEST(WallGeometryTest, SquarePolygonProperties)
{
    std::vector<Vector2> square = { {0,0}, {128,0}, {128,128}, {0,128} };

    EXPECT_GT(signedArea(square), 0);
    EXPECT_TRUE(isConvex(square));
    EXPECT_TRUE(isAxisAlignedRectangle(square));
}

TEST(WallGeometryTest, SimplifyRemovesCollinearVertices)
{
    std::vector<Vector2> polygon = { {0,0}, {64,0}, {128,0}, {128,128}, {0,128} };

    EXPECT_EQ(simplifyCollinear(polygon).size(), 4);
}

TEST(WallGeometryTest, LShapeDecomposesIntoConvexPieces)
{
    std::vector<Vector2> lshape = { {0,0}, {256,0}, {256,128}, {128,128}, {128,256}, {0,256} };

    EXPECT_GT(signedArea(lshape), 0);
    EXPECT_FALSE(isConvex(lshape));
    EXPECT_FALSE(isAxisAlignedRectangle(lshape));

    auto pieces = decomposeIntoConvex(lshape);
    ASSERT_FALSE(pieces.empty());

    double total = 0;

    for (const auto& piece : pieces)
    {
        EXPECT_TRUE(isConvex(piece));
        total += signedArea(piece);
    }

    EXPECT_NEAR(total, signedArea(lshape), 1.0);
}

TEST(WallGeometryTest, DiagonalCutPentagonIsConvex)
{
    std::vector<Vector2> pentagon = { {0,0}, {192,0}, {256,64}, {256,256}, {0,256} };

    EXPECT_TRUE(isConvex(pentagon));
    EXPECT_FALSE(isAxisAlignedRectangle(pentagon));
}

TEST(WallGeometryTest, ConcaveDiagonalPolygonDecomposes)
{
    std::vector<Vector2> concave = { {0,0}, {256,0}, {256,256}, {128,128}, {0,256} };

    EXPECT_FALSE(isConvex(concave));

    auto pieces = decomposeIntoConvex(concave);
    ASSERT_FALSE(pieces.empty());

    double total = 0;

    for (const auto& piece : pieces)
    {
        EXPECT_TRUE(isConvex(piece));
        total += signedArea(piece);
    }

    EXPECT_NEAR(total, signedArea(concave), 1.0);
}

TEST(WallGeometryTest, ButtJointRightAngle)
{
    auto caps = computeButtJointCaps(Vector2(0, 0), Vector2(0, 1), 8.0, Vector2(-1, 0), 8.0);

    ASSERT_TRUE(caps.has_value());

    EXPECT_NEAR(caps->segmentCap.normal.x(), 0, 1e-9);
    EXPECT_NEAR(caps->segmentCap.normal.y(), -1, 1e-9);
    EXPECT_NEAR(caps->segmentCap.dist, -4, 1e-9);

    EXPECT_NEAR(caps->otherCap.normal.x(), 1, 1e-9);
    EXPECT_NEAR(caps->otherCap.normal.y(), 0, 1e-9);
    EXPECT_NEAR(caps->otherCap.dist, 4, 1e-9);
}

TEST(WallGeometryTest, ButtJointOpenDiagonal)
{
    Vector2 diagonal = Vector2(1, 1).getNormalised();

    auto caps = computeButtJointCaps(Vector2(64, 0), diagonal, 8.0, Vector2(-1, 0), 8.0);

    ASSERT_TRUE(caps.has_value());

    EXPECT_NEAR(caps->segmentCap.normal.x(), 0, 1e-9);
    EXPECT_NEAR(caps->segmentCap.normal.y(), -1, 1e-9);
    EXPECT_NEAR(caps->segmentCap.dist, -4, 1e-9);

    EXPECT_NEAR(caps->otherCap.normal.x(), 1, 1e-9);
    EXPECT_NEAR(caps->otherCap.normal.y(), 0, 1e-9);
    EXPECT_NEAR(caps->otherCap.dist, 64 + 4 * (1 + std::sqrt(2.0)), 1e-9);
}

TEST(WallGeometryTest, ButtJointSharpDiagonal)
{
    Vector2 diagonal = Vector2(-1, 1).getNormalised();

    auto caps = computeButtJointCaps(Vector2(64, -32), diagonal, 8.0, Vector2(-1, 0), 8.0);

    ASSERT_TRUE(caps.has_value());

    EXPECT_NEAR(caps->segmentCap.normal.x(), 0, 1e-9);
    EXPECT_NEAR(caps->segmentCap.normal.y(), -1, 1e-9);
    EXPECT_NEAR(caps->segmentCap.dist, 28, 1e-9);

    EXPECT_NEAR(caps->otherCap.normal.x(), 1, 1e-9);
    EXPECT_NEAR(caps->otherCap.normal.y(), 0, 1e-9);
    EXPECT_NEAR(caps->otherCap.dist, 64 + 4 * (std::sqrt(2.0) - 1), 1e-9);
}

TEST(WallGeometryTest, ButtJointRejectsCollinearWalls)
{
    EXPECT_FALSE(computeButtJointCaps(Vector2(0, 0), Vector2(1, 0), 8.0, Vector2(-1, 0), 8.0).has_value());
    EXPECT_FALSE(computeButtJointCaps(Vector2(0, 0), Vector2(1, 0), 8.0, Vector2(1, 0), 8.0).has_value());
}

TEST(WallGeometryTest, DistanceToSegment)
{
    Vector2 a(0, 0);
    Vector2 b(100, 0);

    EXPECT_NEAR(distanceToSegment(Vector2(50, 30), a, b), 30, 1e-9);
    EXPECT_NEAR(distanceToSegment(Vector2(-30, 0), a, b), 30, 1e-9);
    EXPECT_NEAR(distanceToSegment(Vector2(130, 40), a, b), 50, 1e-9);
    EXPECT_NEAR(distanceToSegment(Vector2(50, 0), a, b), 0, 1e-9);
}

}
