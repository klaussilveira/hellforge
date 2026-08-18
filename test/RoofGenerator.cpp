#include "RadiantTest.h"

#include "ibrush.h"
#include "imap.h"
#include "scenelib.h"
#include "math/AABB.h"
#include "math/Vector3.h"

#include "ui/roof/RoofGeometry.h"

namespace test
{

using RoofGeneratorTest = RadiantTest;

namespace
{

const double WALL_TOP = 128;
const double OUTER_X = 512;
const double OUTER_Y = 256;
const double THICKNESS = 16;

AABB boxFromCorners(const Vector3& mins, const Vector3& maxs)
{
    return AABB::createFromMinMax(mins, maxs);
}

std::vector<AABB> makeRoomWalls(double northTop = WALL_TOP)
{
    return {
        boxFromCorners(Vector3(0, OUTER_Y - THICKNESS, 0), Vector3(OUTER_X, OUTER_Y, northTop)),
        boxFromCorners(Vector3(0, 0, 0), Vector3(OUTER_X, THICKNESS, WALL_TOP)),
        boxFromCorners(Vector3(OUTER_X - THICKNESS, THICKNESS, 0),
                       Vector3(OUTER_X, OUTER_Y - THICKNESS, WALL_TOP)),
        boxFromCorners(Vector3(0, THICKNESS, 0),
                       Vector3(THICKNESS, OUTER_Y - THICKNESS, WALL_TOP)),
    };
}

AABB unionBounds(const std::vector<scene::INodePtr>& nodes)
{
    AABB result;

    for (const auto& node : nodes)
    {
        result.includeAABB(node->worldAABB());
    }

    return result;
}

scene::INodePtr nodeWithin(const std::vector<scene::INodePtr>& nodes, int axis,
                           double min, double max)
{
    for (const auto& node : nodes)
    {
        AABB bounds = node->worldAABB();

        if (bounds.origin[axis] - bounds.extents[axis] >= min - 0.01 &&
            bounds.origin[axis] + bounds.extents[axis] <= max + 0.01)
        {
            return node;
        }
    }

    return scene::INodePtr();
}

double minZ(const scene::INodePtr& node)
{
    return node->worldAABB().origin.z() - node->worldAABB().extents.z();
}

double maxZ(const scene::INodePtr& node)
{
    return node->worldAABB().origin.z() + node->worldAABB().extents.z();
}

roof::RoofParams gableParams()
{
    roof::RoofParams params;
    params.type = roof::ROOF_GABLE;
    params.ridgeAxis = roof::RIDGE_AUTO;
    params.height = 64;
    params.slabThickness = 8;
    params.eave = 16;
    params.rake = 8;
    return params;
}

} // anonymous namespace

TEST_F(RoofGeneratorTest, SlopePlaneRisesToRidgeHeight)
{
    Plane3 plane = roof::slopePlane(1, 1.0, 256, 128, 0.5);

    double y = 128;
    double z = (plane.dist() - plane.normal().y() * y) / plane.normal().z();

    EXPECT_NEAR(z, 192, 0.001);
}

TEST_F(RoofGeneratorTest, SlopePlaneKeepsPointsBelowItInside)
{
    Plane3 plane = roof::slopePlane(1, 1.0, 256, 128, 0.5);

    EXPECT_LT(plane.normal().dot(Vector3(0, 128, 191)), plane.dist());
    EXPECT_GT(plane.normal().dot(Vector3(0, 128, 193)), plane.dist());
}

TEST_F(RoofGeneratorTest, BisectorOfOpposingSlopesIsTheRidgePlane)
{
    Plane3 plane = roof::bisectorPlane(1, 1.0, 256, 1, -1.0, 0);

    EXPECT_NEAR(plane.normal().x(), 0, 0.001);
    EXPECT_NEAR(plane.normal().y(), -1, 0.001);
    EXPECT_NEAR(plane.normal().z(), 0, 0.001);
    EXPECT_NEAR(plane.dist(), -128, 0.001);
}

TEST_F(RoofGeneratorTest, BisectorOfAdjacentSlopesIsTheHipLine)
{
    Plane3 plane = roof::bisectorPlane(1, 1.0, 256, 0, 1.0, 512);

    EXPECT_NEAR(plane.normal().dot(Vector3(512, 256, 0)), plane.dist(), 0.001);
    EXPECT_NEAR(plane.normal().dot(Vector3(384, 128, 0)), plane.dist(), 0.001);
}

TEST_F(RoofGeneratorTest, AutoRidgeFollowsTheLongerFootprintAxis)
{
    AABB wide = AABB::createFromMinMax(Vector3(0, 0, 0), Vector3(512, 256, 128));
    AABB deep = AABB::createFromMinMax(Vector3(0, 0, 0), Vector3(256, 512, 128));

    EXPECT_TRUE(roof::ridgeRunsAlongX(wide, roof::RIDGE_AUTO));
    EXPECT_FALSE(roof::ridgeRunsAlongX(deep, roof::RIDGE_AUTO));
}

TEST_F(RoofGeneratorTest, RidgeAxisOverrideBeatsTheFootprint)
{
    AABB wide = AABB::createFromMinMax(Vector3(0, 0, 0), Vector3(512, 256, 128));

    EXPECT_FALSE(roof::ridgeRunsAlongX(wide, roof::RIDGE_ALONG_Y));
    EXPECT_TRUE(roof::ridgeRunsAlongX(wide, roof::RIDGE_ALONG_X));
}

TEST_F(RoofGeneratorTest, SelectWallsDropsFloorAndCeiling)
{
    auto selected = makeRoomWalls();
    selected.push_back(boxFromCorners(Vector3(0, 0, -16), Vector3(OUTER_X, OUTER_Y, 0)));
    selected.push_back(boxFromCorners(Vector3(0, 0, WALL_TOP),
                                      Vector3(OUTER_X, OUTER_Y, WALL_TOP + 16)));

    EXPECT_EQ(roof::selectWalls(selected).size(), 4);
}

TEST_F(RoofGeneratorTest, SelectWallsKeepsASingleBox)
{
    std::vector<AABB> selected{
        boxFromCorners(Vector3(0, 0, 0), Vector3(OUTER_X, OUTER_Y, WALL_TOP))};

    EXPECT_EQ(roof::selectWalls(selected).size(), 1);
}

TEST_F(RoofGeneratorTest, GableCreatesAConnectorPerWallAndTwoSlabs)
{
    auto worldspawn = GlobalMapModule().findOrInsertWorldspawn();
    auto walls = makeRoomWalls();

    auto nodes = roof::generateRoof(walls, roof::footprintOf(walls), gableParams(), worldspawn);

    EXPECT_EQ(nodes.size(), 6);
}

TEST_F(RoofGeneratorTest, HipCreatesAConnectorPerWallAndFourSlabs)
{
    auto worldspawn = GlobalMapModule().findOrInsertWorldspawn();
    auto walls = makeRoomWalls();

    auto params = gableParams();
    params.type = roof::ROOF_HIP;

    auto nodes = roof::generateRoof(walls, roof::footprintOf(walls), params, worldspawn);

    EXPECT_EQ(nodes.size(), 8);
}

TEST_F(RoofGeneratorTest, NothingIsCreatedForADegenerateFootprint)
{
    auto worldspawn = GlobalMapModule().findOrInsertWorldspawn();
    std::vector<AABB> walls{
        boxFromCorners(Vector3(0, 0, 0), Vector3(OUTER_X, 0.5, WALL_TOP))};

    auto nodes = roof::generateRoof(walls, roof::footprintOf(walls), gableParams(), worldspawn);

    EXPECT_TRUE(nodes.empty());
}

TEST_F(RoofGeneratorTest, GableRidgeSitsAtWallTopPlusHeightPlusSlab)
{
    auto worldspawn = GlobalMapModule().findOrInsertWorldspawn();
    auto walls = makeRoomWalls();

    auto nodes = roof::generateRoof(walls, roof::footprintOf(walls), gableParams(), worldspawn);
    AABB bounds = unionBounds(nodes);

    EXPECT_NEAR(bounds.origin.z() + bounds.extents.z(), 200, 0.5);
}

TEST_F(RoofGeneratorTest, GableEaveDropsBelowTheWallTopOutsideTheWall)
{
    auto worldspawn = GlobalMapModule().findOrInsertWorldspawn();
    auto walls = makeRoomWalls();

    auto nodes = roof::generateRoof(walls, roof::footprintOf(walls), gableParams(), worldspawn);
    AABB bounds = unionBounds(nodes);

    EXPECT_NEAR(bounds.origin.y() - bounds.extents.y(), -16, 0.5);
    EXPECT_NEAR(bounds.origin.y() + bounds.extents.y(), 272, 0.5);
    EXPECT_NEAR(bounds.origin.z() - bounds.extents.z(), 120, 0.5);
}

TEST_F(RoofGeneratorTest, GableRakeOverhangsTheGableEnds)
{
    auto worldspawn = GlobalMapModule().findOrInsertWorldspawn();
    auto walls = makeRoomWalls();

    auto nodes = roof::generateRoof(walls, roof::footprintOf(walls), gableParams(), worldspawn);
    AABB bounds = unionBounds(nodes);

    EXPECT_NEAR(bounds.origin.x() - bounds.extents.x(), -8, 0.5);
    EXPECT_NEAR(bounds.origin.x() + bounds.extents.x(), 520, 0.5);
}

TEST_F(RoofGeneratorTest, HipUsesTheEaveOnAllFourSidesAndIgnoresTheRake)
{
    auto worldspawn = GlobalMapModule().findOrInsertWorldspawn();
    auto walls = makeRoomWalls();

    auto params = gableParams();
    params.type = roof::ROOF_HIP;
    params.rake = 100;

    auto nodes = roof::generateRoof(walls, roof::footprintOf(walls), params, worldspawn);
    AABB bounds = unionBounds(nodes);

    EXPECT_NEAR(bounds.origin.x() - bounds.extents.x(), -16, 0.5);
    EXPECT_NEAR(bounds.origin.x() + bounds.extents.x(), 528, 0.5);
    EXPECT_NEAR(bounds.origin.y() - bounds.extents.y(), -16, 0.5);
    EXPECT_NEAR(bounds.origin.y() + bounds.extents.y(), 272, 0.5);
}

TEST_F(RoofGeneratorTest, RidgeAxisOverrideSwapsEaveAndRakeSides)
{
    auto worldspawn = GlobalMapModule().findOrInsertWorldspawn();
    auto walls = makeRoomWalls();

    auto params = gableParams();
    params.ridgeAxis = roof::RIDGE_ALONG_Y;

    auto nodes = roof::generateRoof(walls, roof::footprintOf(walls), params, worldspawn);
    AABB bounds = unionBounds(nodes);

    EXPECT_NEAR(bounds.origin.x() - bounds.extents.x(), -16, 0.5);
    EXPECT_NEAR(bounds.origin.x() + bounds.extents.x(), 528, 0.5);
    EXPECT_NEAR(bounds.origin.y() - bounds.extents.y(), -8, 0.5);
    EXPECT_NEAR(bounds.origin.y() + bounds.extents.y(), 264, 0.5);
}

TEST_F(RoofGeneratorTest, GableEndWallGetsATriangleReachingTheRidge)
{
    auto worldspawn = GlobalMapModule().findOrInsertWorldspawn();
    auto walls = makeRoomWalls();

    auto nodes = roof::generateRoof(walls, roof::footprintOf(walls), gableParams(), worldspawn);

    auto connector = nodeWithin(nodes, 0, OUTER_X - THICKNESS, OUTER_X);
    ASSERT_TRUE(connector != nullptr);

    EXPECT_NEAR(minZ(connector), WALL_TOP, 0.5);
    EXPECT_NEAR(maxZ(connector), 192, 0.5);
}

TEST_F(RoofGeneratorTest, EaveSideWallGetsASlantedConnector)
{
    auto worldspawn = GlobalMapModule().findOrInsertWorldspawn();
    auto walls = makeRoomWalls();

    auto nodes = roof::generateRoof(walls, roof::footprintOf(walls), gableParams(), worldspawn);

    auto connector = nodeWithin(nodes, 1, OUTER_Y - THICKNESS, OUTER_Y);
    ASSERT_TRUE(connector != nullptr);

    EXPECT_NEAR(minZ(connector), WALL_TOP, 0.5);
    EXPECT_NEAR(maxZ(connector), 136, 0.5);
}

TEST_F(RoofGeneratorTest, ShortWallIsExtendedUpToTheRoofUnderside)
{
    auto worldspawn = GlobalMapModule().findOrInsertWorldspawn();
    auto walls = makeRoomWalls(96);

    auto nodes = roof::generateRoof(walls, roof::footprintOf(walls), gableParams(), worldspawn);

    auto connector = nodeWithin(nodes, 1, OUTER_Y - THICKNESS, OUTER_Y);
    ASSERT_TRUE(connector != nullptr);

    EXPECT_NEAR(minZ(connector), 96, 0.5);
    EXPECT_NEAR(maxZ(connector), 136, 0.5);
}

TEST_F(RoofGeneratorTest, ConnectorsMeetTheSlabsAtTheWallLine)
{
    auto worldspawn = GlobalMapModule().findOrInsertWorldspawn();
    auto walls = makeRoomWalls();

    auto params = gableParams();
    params.eave = 0;
    params.rake = 0;

    auto nodes = roof::generateRoof(walls, roof::footprintOf(walls), params, worldspawn);

    AABB bounds = unionBounds(nodes);
    EXPECT_NEAR(bounds.origin.z() - bounds.extents.z(), WALL_TOP, 0.5);
}

} // namespace test
