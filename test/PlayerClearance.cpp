#include "RadiantTest.h"

#include "ieclass.h"
#include "imap.h"

#include "scene/EntityNode.h"
#include "algorithm/Entity.h"
#include "algorithm/Primitives.h"
#include "camera/tools/PlayerClearanceTool.h"

namespace test
{

using PlayerClearanceTest = RadiantTest;

namespace
{

const std::string SolidMaterial = "textures/parsertest/surfaceflags/solid";

ui::playerClearance::Dimensions doom3Dimensions()
{
    ui::playerClearance::Dimensions dims;
    dims.width = 32;
    dims.standHeight = 74;
    dims.crouchHeight = 38;
    dims.standViewHeight = 68;
    dims.crouchViewHeight = 32;
    dims.minWalkNormal = 0.7;
    return dims;
}

scene::INodePtr createSlab(const scene::INodePtr& parent, double bottom, double top, const std::string& material)
{
    double centre = (bottom + top) * 0.5;
    double half = (top - bottom) * 0.5;
    return algorithm::createCuboidBrush(parent, AABB(Vector3(0, 0, centre), Vector3(256, 256, half)), material);
}

EntityNodePtr createBrushEntity(const std::string& className)
{
    auto eclass = GlobalEntityClassManager().findOrInsert(className, true);
    auto entity = GlobalEntityModule().createEntity(eclass);
    GlobalMapModule().getRoot()->addChildNode(entity);
    return entity;
}

ui::playerClearance::Fit classifyAtOrigin()
{
    return ui::playerClearance::classify(GlobalMapModule().getRoot(), Vector3(0, 0, 0.25), doom3Dimensions());
}

}

TEST_F(PlayerClearanceTest, DimensionsMissingFromGameConfig)
{
    EXPECT_FALSE(ui::playerClearance::loadDimensions().isValid());
}

TEST_F(PlayerClearanceTest, DimensionsValidity)
{
    EXPECT_TRUE(doom3Dimensions().isValid());

    auto dims = doom3Dimensions();
    dims.crouchHeight = 0;
    EXPECT_FALSE(dims.isValid());
}

TEST_F(PlayerClearanceTest, MaterialBlocksPlayer)
{
    using ui::playerClearance::materialBlocksPlayer;

    EXPECT_TRUE(materialBlocksPlayer("textures/common/caulk"));
    EXPECT_TRUE(materialBlocksPlayer(SolidMaterial));
    EXPECT_TRUE(materialBlocksPlayer("textures/parsertest/surfaceflags/playerclip"));
    EXPECT_TRUE(materialBlocksPlayer("textures/parsertest/surfaceflags/monsterclip"));

    EXPECT_FALSE(materialBlocksPlayer("textures/parsertest/surfaceflags/nonsolid"));
    EXPECT_FALSE(materialBlocksPlayer("textures/parsertest/surfaceflags/water"));
    EXPECT_FALSE(materialBlocksPlayer("textures/parsertest/surfaceflags/areaportal"));
    EXPECT_FALSE(materialBlocksPlayer("textures/parsertest/surfaceflags/nocarve"));
}

TEST_F(PlayerClearanceTest, HullBounds)
{
    auto hull = ui::playerClearance::hullAt(Vector3(10, 20, 30), 32, 74);

    EXPECT_EQ(hull.getOrigin(), Vector3(10, 20, 67));
    EXPECT_EQ(hull.getExtents(), Vector3(16, 16, 37));
}

TEST_F(PlayerClearanceTest, PlaceOnFlatFloor)
{
    Vector3 origin;

    EXPECT_TRUE(ui::playerClearance::placeOnFloor(Vector3(5, 6, 7), Vector3(0, 0, 1), doom3Dimensions(), origin));
    EXPECT_EQ(origin, Vector3(5, 6, 7.25));
}

TEST_F(PlayerClearanceTest, PlaceOnSlopeLiftsCorners)
{
    Vector3 normal = Vector3(1, 0, 1).getNormalised();
    Vector3 origin;

    EXPECT_TRUE(ui::playerClearance::placeOnFloor(Vector3(0, 0, 0), normal, doom3Dimensions(), origin));
    EXPECT_NEAR(origin.z(), 16.25, 1e-6);
}

TEST_F(PlayerClearanceTest, PlaceRejectsUnwalkableSurfaces)
{
    auto dims = doom3Dimensions();
    Vector3 origin;

    EXPECT_FALSE(ui::playerClearance::placeOnFloor(Vector3(0, 0, 0), Vector3(1, 0, 0), dims, origin));
    EXPECT_FALSE(ui::playerClearance::placeOnFloor(Vector3(0, 0, 0), Vector3(0, 0, -1), dims, origin));
    EXPECT_FALSE(ui::playerClearance::placeOnFloor(Vector3(0, 0, 0), Vector3(1.5, 0, 1).getNormalised(), dims, origin));
}

TEST_F(PlayerClearanceTest, StandsInOpenSpaceOnFloor)
{
    auto worldspawn = GlobalMapModule().findOrInsertWorldspawn();
    createSlab(worldspawn, -16, 0, SolidMaterial);

    EXPECT_EQ(classifyAtOrigin(), ui::playerClearance::Fit::Stand);
}

TEST_F(PlayerClearanceTest, LowCeilingForcesCrouch)
{
    auto worldspawn = GlobalMapModule().findOrInsertWorldspawn();
    createSlab(worldspawn, 50, 66, SolidMaterial);

    EXPECT_EQ(classifyAtOrigin(), ui::playerClearance::Fit::Crouch);
}

TEST_F(PlayerClearanceTest, VeryLowCeilingBlocks)
{
    auto worldspawn = GlobalMapModule().findOrInsertWorldspawn();
    createSlab(worldspawn, 30, 46, SolidMaterial);

    EXPECT_EQ(classifyAtOrigin(), ui::playerClearance::Fit::Blocked);
}

TEST_F(PlayerClearanceTest, CeilingExactlyAtStandHeightFits)
{
    auto worldspawn = GlobalMapModule().findOrInsertWorldspawn();
    createSlab(worldspawn, 74.25, 90, SolidMaterial);

    EXPECT_EQ(classifyAtOrigin(), ui::playerClearance::Fit::Stand);
}

TEST_F(PlayerClearanceTest, FlushWallDoesNotBlock)
{
    auto worldspawn = GlobalMapModule().findOrInsertWorldspawn();
    algorithm::createCuboidBrush(worldspawn, AABB(Vector3(32, 0, 64), Vector3(16, 128, 64)), SolidMaterial);

    EXPECT_EQ(classifyAtOrigin(), ui::playerClearance::Fit::Stand);
}

TEST_F(PlayerClearanceTest, OverlappingWallBlocks)
{
    auto worldspawn = GlobalMapModule().findOrInsertWorldspawn();
    algorithm::createCuboidBrush(worldspawn, AABB(Vector3(30, 0, 64), Vector3(16, 128, 64)), SolidMaterial);

    EXPECT_EQ(classifyAtOrigin(), ui::playerClearance::Fit::Blocked);
}

TEST_F(PlayerClearanceTest, NonsolidCeilingDoesNotBlock)
{
    auto worldspawn = GlobalMapModule().findOrInsertWorldspawn();
    createSlab(worldspawn, 30, 46, "textures/parsertest/surfaceflags/nonsolid");

    EXPECT_EQ(classifyAtOrigin(), ui::playerClearance::Fit::Stand);
}

TEST_F(PlayerClearanceTest, PlayerclipCeilingBlocks)
{
    auto worldspawn = GlobalMapModule().findOrInsertWorldspawn();
    createSlab(worldspawn, 50, 66, "textures/parsertest/surfaceflags/playerclip");

    EXPECT_EQ(classifyAtOrigin(), ui::playerClearance::Fit::Crouch);
}

TEST_F(PlayerClearanceTest, TriggerEntityDoesNotBlock)
{
    auto trigger = createBrushEntity("trigger_multiple");
    createSlab(trigger, 30, 46, SolidMaterial);

    EXPECT_EQ(classifyAtOrigin(), ui::playerClearance::Fit::Stand);
}

TEST_F(PlayerClearanceTest, NonSolidEntityDoesNotBlock)
{
    auto funcStatic = createBrushEntity("func_static");
    funcStatic->getEntity().setKeyValue("solid", "0");
    createSlab(funcStatic, 30, 46, SolidMaterial);

    EXPECT_EQ(classifyAtOrigin(), ui::playerClearance::Fit::Stand);
}

TEST_F(PlayerClearanceTest, SolidEntityBlocks)
{
    auto funcStatic = createBrushEntity("func_static");
    createSlab(funcStatic, 30, 46, SolidMaterial);

    EXPECT_EQ(classifyAtOrigin(), ui::playerClearance::Fit::Blocked);
}

}
