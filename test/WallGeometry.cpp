#include "gtest/gtest.h"

#include <cmath>

#include "polygon/Polygon2D.h"
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

    EXPECT_GT(polygon::ringArea(square), 0);
    EXPECT_TRUE(polygon::isConvex(square));
}

TEST(WallGeometryTest, SimplifyRemovesCollinearVertices)
{
    std::vector<Vector2> outline = { {0,0}, {64,0}, {128,0}, {128,128}, {0,128} };

    std::vector<polygon::Ring> rings = polygon::repair({ outline }, polygon::EPSILON);

    ASSERT_EQ(rings.size(), 1u);
    EXPECT_EQ(rings.front().size(), 4u);
}

TEST(WallGeometryTest, LShapeDecomposesIntoConvexPieces)
{
    std::vector<Vector2> lshape = { {0,0}, {256,0}, {256,128}, {128,128}, {128,256}, {0,256} };

    EXPECT_GT(polygon::ringArea(lshape), 0);
    EXPECT_FALSE(polygon::isConvex(lshape));

    auto pieces = polygon::convexPieces({ lshape }, polygon::EPSILON);
    ASSERT_FALSE(pieces.empty());

    double total = 0;

    for (const auto& piece : pieces)
    {
        EXPECT_TRUE(polygon::isConvex(piece));
        total += polygon::ringArea(piece);
    }

    EXPECT_NEAR(total, polygon::ringArea(lshape), 1.0);
}

TEST(WallGeometryTest, DiagonalCutPentagonIsConvex)
{
    std::vector<Vector2> pentagon = { {0,0}, {192,0}, {256,64}, {256,256}, {0,256} };

    EXPECT_TRUE(polygon::isConvex(pentagon));
}

TEST(WallGeometryTest, ConcaveDiagonalPolygonDecomposes)
{
    std::vector<Vector2> concave = { {0,0}, {256,0}, {256,256}, {128,128}, {0,256} };

    EXPECT_FALSE(polygon::isConvex(concave));

    auto pieces = polygon::convexPieces({ concave }, polygon::EPSILON);
    ASSERT_FALSE(pieces.empty());

    double total = 0;

    for (const auto& piece : pieces)
    {
        EXPECT_TRUE(polygon::isConvex(piece));
        total += polygon::ringArea(piece);
    }

    EXPECT_NEAR(total, polygon::ringArea(concave), 1.0);
}

polygon::Ring wall(double ax, double ay, double bx, double by)
{
    return polygon::Ring{ Vector2(ax, ay), Vector2(bx, by) };
}

std::vector<WallLine> linesOf(const std::vector<polygon::Ring>& walls, double thickness)
{
    std::vector<WallLine> lines;

    for (const polygon::Ring& w : walls)
    {
        lines.push_back({ w.front(), w.back(), thickness });
    }

    return lines;
}

std::vector<polygon::Ring> footprintOf(const std::vector<polygon::Ring>& walls, double thickness)
{
    return levelFootprint(linesOf(walls, thickness));
}

std::vector<polygon::Ring> carvedFootprintOf(const std::vector<polygon::Ring>& walls,
    double thickness)
{
    return polygon::difference(footprintOf(walls, thickness),
                               corridorMouths(linesOf(walls, thickness)));
}

std::vector<polygon::Region> walkableOf(const std::vector<polygon::Ring>& walls, double thickness)
{
    auto lines = linesOf(walls, thickness);
    return walkableRegions(lines, corridorMouths(lines));
}

std::vector<polygon::Ring> roomsOf(const std::vector<polygon::Ring>& walls, double thickness)
{
    std::vector<polygon::Ring> rooms;

    for (const polygon::Region& region : polygon::regions(footprintOf(walls, thickness)))
    {
        rooms.insert(rooms.end(), region.holes.begin(), region.holes.end());
    }

    return rooms;
}

double totalArea(const std::vector<polygon::Ring>& rings)
{
    double total = 0;

    for (const polygon::Ring& ring : rings)
    {
        total += std::abs(polygon::ringArea(ring));
    }

    return total;
}

bool hasVertex(const std::vector<polygon::Ring>& rings, const Vector2& point)
{
    for (const polygon::Ring& ring : rings)
    {
        for (const Vector2& vertex : ring)
        {
            if (std::abs(vertex.x() - point.x()) < 0.02 &&
                std::abs(vertex.y() - point.y()) < 0.02)
            {
                return true;
            }
        }
    }

    return false;
}

std::vector<polygon::Ring> closedRoom()
{
    return { wall(0,0, 256,0), wall(256,0, 256,256), wall(256,256, 0,256), wall(0,256, 0,0) };
}

TEST(WallGeometryTest, FreeStandingWallKeepsItsDrawnLength)
{
    auto footprint = footprintOf({ wall(0,0, 256,0) }, 8);

    EXPECT_NEAR(totalArea(footprint), 256.0 * 8.0, 0.5);
    EXPECT_TRUE(hasVertex(footprint, Vector2(0, -4)));
    EXPECT_TRUE(hasVertex(footprint, Vector2(256, 4)));
}

TEST(WallGeometryTest, JoinedCornerIsSquaredOffNotNotched)
{
    auto footprint = footprintOf({ wall(0,0, 256,0), wall(256,0, 256,256) }, 8);

    EXPECT_NEAR(totalArea(footprint), 4096.0, 0.5);
    EXPECT_TRUE(hasVertex(footprint, Vector2(260, -4)));
}

TEST(WallGeometryTest, ThreeWayJunctionMergesWithoutOverlap)
{
    auto footprint = footprintOf(
        { wall(0,0, 128,0), wall(128,0, 256,0), wall(128,0, 128,128) }, 8);

    EXPECT_NEAR(totalArea(footprint), 256.0 * 8.0 + 128.0 * 8.0 - 8.0 * 4.0, 0.5);
    EXPECT_TRUE(roomsOf({ wall(0,0, 128,0), wall(128,0, 256,0), wall(128,0, 128,128) }, 8).empty());
}

TEST(WallGeometryTest, ClosedLoopEnclosesOneRoomAtTheInnerWallFaces)
{
    auto rooms = roomsOf(closedRoom(), 8);

    ASSERT_EQ(rooms.size(), 1u);
    EXPECT_NEAR(totalArea(rooms), 248.0 * 248.0, 0.5);
}

TEST(WallGeometryTest, OpenLoopEnclosesNothing)
{
    auto walls = closedRoom();
    walls.pop_back();

    EXPECT_TRUE(roomsOf(walls, 8).empty());
}

TEST(WallGeometryTest, DividerWallSplitsOneRoomIntoTwo)
{
    auto walls = closedRoom();
    walls.push_back(wall(128,0, 128,256));

    auto rooms = roomsOf(walls, 8);

    ASSERT_EQ(rooms.size(), 2u);
    EXPECT_NEAR(totalArea(rooms), 2.0 * 120.0 * 248.0, 0.5);
}

TEST(WallGeometryTest, ExtendingOffAnExistingRoomAddsASecondRoom)
{
    auto walls = closedRoom();

    EXPECT_EQ(roomsOf(walls, 8).size(), 1u);

    walls.push_back(wall(256,0, 512,0));
    walls.push_back(wall(512,0, 512,256));
    walls.push_back(wall(512,256, 256,256));

    auto rooms = roomsOf(walls, 8);

    ASSERT_EQ(rooms.size(), 2u);
    EXPECT_NEAR(totalArea(rooms), 248.0 * 248.0 + 248.0 * 248.0, 0.5);
}

TEST(WallGeometryTest, RoomInsideARoomIsItsOwnRoom)
{
    auto walls = closedRoom();
    walls.push_back(wall(64,64, 192,64));
    walls.push_back(wall(192,64, 192,192));
    walls.push_back(wall(192,192, 64,192));
    walls.push_back(wall(64,192, 64,64));

    EXPECT_EQ(roomsOf(walls, 8).size(), 2u);
}

bool hasCollinearVertices(const polygon::Ring& ring)
{
    for (std::size_t i = 0; i < ring.size(); ++i)
    {
        const Vector2& previous = ring[(i + ring.size() - 1) % ring.size()];
        const Vector2& current = ring[i];
        const Vector2& next = ring[(i + 1) % ring.size()];

        Vector2 span = next - previous;
        double length = span.getLength();

        if (length <= polygon::EPSILON)
        {
            return true;
        }

        if (std::abs(span.crossProduct(current - previous)) / length <= polygon::EPSILON)
        {
            return true;
        }
    }

    return false;
}

bool covers(const std::vector<polygon::Ring>& pieces, const Vector2& point)
{
    for (const polygon::Ring& piece : pieces)
    {
        Clipper2Lib::PathD path;

        for (const Vector2& vertex : piece)
        {
            path.push_back(Clipper2Lib::PointD(vertex.x(), vertex.y()));
        }

        if (Clipper2Lib::PointInPolygon(Clipper2Lib::PointD(point.x(), point.y()), path) !=
            Clipper2Lib::PointInPolygonResult::IsOutside)
        {
            return true;
        }
    }

    return false;
}

std::vector<polygon::Ring> roomWithCorridor()
{
    return { wall(-80,-128, 128,-128), wall(128,-128, 128,256),
             wall(128,256, -80,256), wall(-80,256, -80,-128),
             wall(-80,112, -344,112), wall(-80,32, -344,32) };
}

TEST(WallGeometryTest, ConvexPiecesNeverCarryCollinearVertices)
{
    auto pieces = polygon::convexPieces(footprintOf(roomWithCorridor(), 8), polygon::EPSILON);

    ASSERT_FALSE(pieces.empty());

    for (const polygon::Ring& piece : pieces)
    {
        EXPECT_FALSE(hasCollinearVertices(piece));
    }
}

TEST(WallGeometryTest, WallJoinedByACorridorSurvivesItsWholeLength)
{
    auto walls = roomWithCorridor();
    auto pieces = polygon::convexPieces(footprintOf(walls, 8), polygon::EPSILON);

    EXPECT_NEAR(totalArea(pieces), 9472.0 + 4224.0 - 64.0, 0.5);

    EXPECT_TRUE(covers(pieces, Vector2(-80, 200)));
    EXPECT_TRUE(covers(pieces, Vector2(-80, -50)));
    EXPECT_TRUE(covers(pieces, Vector2(-80, 72)));
    EXPECT_TRUE(covers(pieces, Vector2(-200, 112)));
}

TEST(WallGeometryTest, GableHalvesFollowAnLShapedRoomNotItsBoundingBox)
{
    polygon::Ring lshape = { {0,0}, {256,0}, {256,128}, {128,128}, {128,256}, {0,256} };

    polygon::Ring lowHalf = { {-1,-1}, {257,-1}, {257,128}, {-1,128} };
    polygon::Ring highHalf = { {-1,128}, {257,128}, {257,257}, {-1,257} };

    auto low = polygon::intersect({ lshape }, { lowHalf });
    auto high = polygon::intersect({ lshape }, { highHalf });

    ASSERT_FALSE(low.empty());
    ASSERT_FALSE(high.empty());

    EXPECT_NEAR(totalArea(low), 256.0 * 128.0, 0.5);
    EXPECT_NEAR(totalArea(high), 128.0 * 128.0, 0.5);

    EXPECT_NEAR(totalArea(low) + totalArea(high), 49152.0, 0.5);
    EXPECT_LT(totalArea(low) + totalArea(high), 256.0 * 256.0);
}

TEST(WallGeometryTest, RoofPiecesOfAnLShapedRoomAreConvexAndCollinearFree)
{
    polygon::Ring lshape = { {0,0}, {256,0}, {256,128}, {128,128}, {128,256}, {0,256} };
    polygon::Ring highHalf = { {-1,128}, {257,128}, {257,257}, {-1,257} };

    auto pieces = polygon::convexPieces(
        polygon::intersect({ lshape }, { highHalf }), polygon::EPSILON);

    ASSERT_FALSE(pieces.empty());

    for (const polygon::Ring& piece : pieces)
    {
        EXPECT_TRUE(polygon::isConvex(piece));
        EXPECT_FALSE(hasCollinearVertices(piece));
    }

    EXPECT_NEAR(totalArea(pieces), 128.0 * 128.0, 0.5);
}

TEST(WallGeometryTest, CorridorMouthOpensTheWallItRunsInto)
{
    auto walls = roomWithCorridor();
    auto mouths = corridorMouths(linesOf(walls, 8));

    ASSERT_EQ(mouths.size(), 1u);

    EXPECT_NEAR(totalArea(mouths), 72.0 * 8.0, 0.5);

    auto carved = carvedFootprintOf(walls, 8);
    auto pieces = polygon::convexPieces(carved, polygon::EPSILON);

    EXPECT_NEAR(totalArea(pieces), 9472.0 + 4224.0 - 64.0 - 576.0, 0.5);

    EXPECT_FALSE(covers(pieces, Vector2(-80, 72)));
    EXPECT_TRUE(covers(pieces, Vector2(-80, 200)));
    EXPECT_TRUE(covers(pieces, Vector2(-80, -50)));
    EXPECT_TRUE(covers(pieces, Vector2(-80, 112)));
    EXPECT_TRUE(covers(pieces, Vector2(-80, 32)));
}

TEST(WallGeometryTest, ASingleWallRunningIntoAnotherOpensNothing)
{
    std::vector<polygon::Ring> walls = {
        wall(-80,-128, 128,-128), wall(128,-128, 128,256),
        wall(128,256, -80,256), wall(-80,256, -80,-128),
        wall(-80,112, -344,112) };

    EXPECT_TRUE(corridorMouths(linesOf(walls, 8)).empty());
}

TEST(WallGeometryTest, WallsMeetingAtACornerAreNotTreatedAsADoorway)
{
    std::vector<polygon::Ring> walls = {
        wall(0,0, 256,0), wall(0,0, 0,-128), wall(256,0, 256,-128) };

    EXPECT_TRUE(corridorMouths(linesOf(walls, 8)).empty());
}

TEST(WallGeometryTest, OpeningACorridorMouthKeepsTheRoomItOpensInto)
{
    auto walls = roomWithCorridor();

    EXPECT_EQ(roomsOf(walls, 8).size(), 1u);
    EXPECT_NEAR(totalArea(roomsOf(walls, 8)), 200.0 * 376.0, 0.5);
}

TEST(WallGeometryTest, FloorAndCeilingRunThroughTheCorridorMouth)
{
    auto walls = roomWithCorridor();
    auto walkable = walkableOf(walls, 8);

    ASSERT_EQ(walkable.size(), 1u);
    EXPECT_TRUE(walkable[0].holes.empty());

    double floorArea = std::abs(polygon::ringArea(walkable[0].outer));

    EXPECT_NEAR(floorArea, 200.0 * 376.0 + 72.0 * 8.0, 0.5);
    EXPECT_GT(floorArea, totalArea(roomsOf(walls, 8)));

    auto pieces = polygon::convexPieces({ walkable[0].outer }, polygon::EPSILON);

    ASSERT_FALSE(pieces.empty());
    EXPECT_TRUE(covers(pieces, Vector2(-80, 72)));
    EXPECT_TRUE(covers(pieces, Vector2(-76, 72)));
    EXPECT_TRUE(covers(pieces, Vector2(-84, 72)));
    EXPECT_NEAR(totalArea(pieces), floorArea, 0.5);
}

TEST(WallGeometryTest, RoomAndClosedCorridorShareOneContinuousFloor)
{
    auto walls = roomWithCorridor();

    walls.push_back(wall(-344,112, -344,32));

    ASSERT_EQ(roomsOf(walls, 8).size(), 2u);

    auto walkable = walkableOf(walls, 8);

    ASSERT_EQ(walkable.size(), 1u);
    EXPECT_TRUE(walkable[0].holes.empty());

    double room = 200.0 * 376.0;
    double corridor = 256.0 * 72.0;
    double mouth = 72.0 * 8.0;

    EXPECT_NEAR(std::abs(polygon::ringArea(walkable[0].outer)), room + corridor + mouth, 0.5);

    auto pieces = polygon::convexPieces({ walkable[0].outer }, polygon::EPSILON);

    EXPECT_NEAR(totalArea(pieces), room + corridor + mouth, 0.5);
    EXPECT_TRUE(covers(pieces, Vector2(-80, 72)));
    EXPECT_TRUE(covers(pieces, Vector2(-200, 72)));
    EXPECT_TRUE(covers(pieces, Vector2(0, 0)));
}

TEST(WallGeometryTest, ADoorwayWithNoRoomBehindItGetsNoFloor)
{
    auto walls = closedRoom();

    walls.push_back(wall(-512,0, -512,256));
    walls.push_back(wall(-512,112, -776,112));
    walls.push_back(wall(-512,32, -776,32));

    ASSERT_EQ(corridorMouths(linesOf(walls, 8)).size(), 1u);

    auto walkable = walkableOf(walls, 8);

    ASSERT_EQ(walkable.size(), 1u);
    EXPECT_NEAR(std::abs(polygon::ringArea(walkable[0].outer)), 248.0 * 248.0, 0.5);
}

TEST(WallGeometryTest, WallShorterThanItsThicknessStillProducesGeometry)
{
    EXPECT_FALSE(footprintOf({ wall(0,0, 4,0) }, 8).empty());
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
