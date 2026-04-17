#include "RadiantTest.h"

#include "icommandsystem.h"
#include "iselection.h"
#include "ientity.h"
#include "ieclass.h"
#include "imap.h"
#include "ipatch.h"
#include "scene/EntityNode.h"
#include "scenelib.h"

namespace test
{

using PatchSewingTest = RadiantTest;

namespace
{

scene::INodePtr createPatch(const scene::INodePtr& parent,
                            std::size_t width, std::size_t height,
                            double originX, double originY,
                            double stepX, double stepY,
                            double z = 0.0)
{
    auto sceneNode = GlobalPatchModule().createPatch(patch::PatchDefType::Def2);
    parent->addChildNode(sceneNode);

    auto patch = Node_getIPatch(sceneNode);
    patch->setDims(width, height);
    patch->setShader("_default");

    for (std::size_t row = 0; row < height; ++row)
    {
        for (std::size_t col = 0; col < width; ++col)
        {
            patch->ctrlAt(row, col).vertex.set(
                originX + col * stepX,
                originY + row * stepY,
                z);
        }
    }

    patch->controlPointsChanged();
    return sceneNode;
}

IPatch& patchOf(const scene::INodePtr& node)
{
    return *Node_getIPatch(node);
}

}

TEST_F(PatchSewingTest, SewAdjacentPatchesWithDriftedEdge)
{
    auto worldspawn = GlobalMapModule().findOrInsertWorldspawn();

    auto patchA = createPatch(worldspawn, 3, 3, 0.0, 0.0, 32.0, 32.0);
    auto patchB = createPatch(worldspawn, 3, 3, 64.0, 0.0, 32.0, 32.0);

    // Drift patchB's left column (col 0) upward by 4 units in Z
    for (std::size_t row = 0; row < 3; ++row)
    {
        patchOf(patchB).ctrlAt(row, 0).vertex.z() += 4.0;
    }
    patchOf(patchB).controlPointsChanged();

    Node_setSelected(patchA, true);
    Node_setSelected(patchB, true);

    GlobalCommandSystem().executeCommand("SewSelectedPatches");

    // Edge should now be at midpoint z = 2.0 on both sides
    for (std::size_t row = 0; row < 3; ++row)
    {
        EXPECT_NEAR(patchOf(patchA).ctrlAt(row, 2).vertex.z(), 2.0, 0.001);
        EXPECT_NEAR(patchOf(patchB).ctrlAt(row, 0).vertex.z(), 2.0, 0.001);
        EXPECT_TRUE(math::isNear(
            patchOf(patchA).ctrlAt(row, 2).vertex,
            patchOf(patchB).ctrlAt(row, 0).vertex, 0.001));
    }
}

TEST_F(PatchSewingTest, SewRejectsDriftBeyondTolerance)
{
    auto worldspawn = GlobalMapModule().findOrInsertWorldspawn();

    // Wide patchB (step x = 200) so only its left column is close to patchA's right edge,
    // and that one column is drifted beyond the default tolerance of 64.
    auto patchA = createPatch(worldspawn, 3, 3, 0.0, 0.0, 32.0, 32.0);
    auto patchB = createPatch(worldspawn, 3, 3, 64.0, 0.0, 200.0, 32.0);

    for (std::size_t row = 0; row < 3; ++row)
    {
        patchOf(patchB).ctrlAt(row, 0).vertex.z() += 100.0;
    }
    patchOf(patchB).controlPointsChanged();

    const Vector3 originalA = patchOf(patchA).ctrlAt(0, 2).vertex;
    const Vector3 originalB = patchOf(patchB).ctrlAt(0, 0).vertex;

    Node_setSelected(patchA, true);
    Node_setSelected(patchB, true);

    GlobalCommandSystem().executeCommand("SewSelectedPatches");

    EXPECT_TRUE(math::isNear(patchOf(patchA).ctrlAt(0, 2).vertex, originalA, 0.001));
    EXPECT_TRUE(math::isNear(patchOf(patchB).ctrlAt(0, 0).vertex, originalB, 0.001));
}

TEST_F(PatchSewingTest, SewWithCustomTolerance)
{
    auto worldspawn = GlobalMapModule().findOrInsertWorldspawn();

    auto patchA = createPatch(worldspawn, 3, 3, 0.0, 0.0, 32.0, 32.0);
    // Use a wider step for patchB so its far column stays well clear of patchA
    auto patchB = createPatch(worldspawn, 3, 3, 64.0, 0.0, 64.0, 32.0);

    // Drift patchB's left column by 80 units in z (beyond default 64 tolerance,
    // but closer to patchA's right edge than patchB's far column at x=192)
    for (std::size_t row = 0; row < 3; ++row)
    {
        patchOf(patchB).ctrlAt(row, 0).vertex.z() += 80.0;
    }
    patchOf(patchB).controlPointsChanged();

    Node_setSelected(patchA, true);
    Node_setSelected(patchB, true);

    // Explicit tolerance of 256 covers the 80-unit drift
    GlobalCommandSystem().executeCommand("SewSelectedPatches", cmd::Argument(256.0));

    for (std::size_t row = 0; row < 3; ++row)
    {
        EXPECT_NEAR(patchOf(patchA).ctrlAt(row, 2).vertex.z(), 40.0, 0.001);
        EXPECT_NEAR(patchOf(patchB).ctrlAt(row, 0).vertex.z(), 40.0, 0.001);
    }
}

TEST_F(PatchSewingTest, SewRejectsFarApartPatches)
{
    auto worldspawn = GlobalMapModule().findOrInsertWorldspawn();

    // Place patches far apart so no edge pair lies within tolerance.
    auto patchA = createPatch(worldspawn, 3, 3, 0.0, 0.0, 32.0, 32.0);
    auto patchB = createPatch(worldspawn, 3, 3, 1000.0, 1000.0, 32.0, 32.0);

    const Vector3 originalA = patchOf(patchA).ctrlAt(0, 2).vertex;
    const Vector3 originalB = patchOf(patchB).ctrlAt(0, 0).vertex;

    Node_setSelected(patchA, true);
    Node_setSelected(patchB, true);

    GlobalCommandSystem().executeCommand("SewSelectedPatches");

    EXPECT_TRUE(math::isNear(patchOf(patchA).ctrlAt(0, 2).vertex, originalA, 0.001));
    EXPECT_TRUE(math::isNear(patchOf(patchB).ctrlAt(0, 0).vertex, originalB, 0.001));
}

TEST_F(PatchSewingTest, SewSkipsAlreadyCoincidentEdges)
{
    auto worldspawn = GlobalMapModule().findOrInsertWorldspawn();

    auto patchA = createPatch(worldspawn, 3, 3, 0.0, 0.0, 32.0, 32.0);
    auto patchB = createPatch(worldspawn, 3, 3, 64.0, 0.0, 32.0, 32.0);

    // Edges are already coincident at x=64. Sew should skip (no-op) and report failure;
    // positions must remain exactly as initialised.
    const Vector3 originalA = patchOf(patchA).ctrlAt(0, 2).vertex;
    const Vector3 originalB = patchOf(patchB).ctrlAt(0, 0).vertex;

    Node_setSelected(patchA, true);
    Node_setSelected(patchB, true);

    GlobalCommandSystem().executeCommand("SewSelectedPatches");

    EXPECT_TRUE(math::isNear(patchOf(patchA).ctrlAt(0, 2).vertex, originalA, 0.0001));
    EXPECT_TRUE(math::isNear(patchOf(patchB).ctrlAt(0, 0).vertex, originalB, 0.0001));
}

TEST_F(PatchSewingTest, SewMatchesReverseOrientation)
{
    auto worldspawn = GlobalMapModule().findOrInsertWorldspawn();

    auto patchA = createPatch(worldspawn, 3, 3, 0.0, 0.0, 32.0, 32.0);
    auto patchB = createPatch(worldspawn, 3, 3, 64.0, 0.0, 32.0, 32.0);

    // patchA col 2 iterated row 0..2: y = 0, 32, 64
    // Set patchB col 0 with rows running opposite Y direction so that only
    // a reverse-orientation pairing lines the edges up (and drifted in z by 2).
    patchOf(patchB).ctrlAt(0, 0).vertex.set(64.0, 64.0, 2.0);
    patchOf(patchB).ctrlAt(1, 0).vertex.set(64.0, 32.0, 2.0);
    patchOf(patchB).ctrlAt(2, 0).vertex.set(64.0,  0.0, 2.0);
    patchOf(patchB).controlPointsChanged();

    Node_setSelected(patchA, true);
    Node_setSelected(patchB, true);

    GlobalCommandSystem().executeCommand("SewSelectedPatches");

    // After sew via reverse match: patchA(row, col=2) pairs with patchB(2-row, col=0).
    // All midpoints land at z = 1.0; XY stays put.
    EXPECT_TRUE(math::isNear(patchOf(patchA).ctrlAt(0, 2).vertex, Vector3(64.0,  0.0, 1.0), 0.001));
    EXPECT_TRUE(math::isNear(patchOf(patchA).ctrlAt(1, 2).vertex, Vector3(64.0, 32.0, 1.0), 0.001));
    EXPECT_TRUE(math::isNear(patchOf(patchA).ctrlAt(2, 2).vertex, Vector3(64.0, 64.0, 1.0), 0.001));

    EXPECT_TRUE(math::isNear(patchOf(patchB).ctrlAt(0, 0).vertex, Vector3(64.0, 64.0, 1.0), 0.001));
    EXPECT_TRUE(math::isNear(patchOf(patchB).ctrlAt(1, 0).vertex, Vector3(64.0, 32.0, 1.0), 0.001));
    EXPECT_TRUE(math::isNear(patchOf(patchB).ctrlAt(2, 0).vertex, Vector3(64.0,  0.0, 1.0), 0.001));
}

TEST_F(PatchSewingTest, SewAcrossMultipleSelectedPatches)
{
    auto worldspawn = GlobalMapModule().findOrInsertWorldspawn();

    auto patchA = createPatch(worldspawn, 3, 3, 0.0, 0.0, 32.0, 32.0);
    auto patchB = createPatch(worldspawn, 3, 3, 64.0, 0.0, 32.0, 32.0);
    auto patchC = createPatch(worldspawn, 3, 3, 1000.0, 1000.0, 32.0, 32.0);

    for (std::size_t row = 0; row < 3; ++row)
    {
        patchOf(patchB).ctrlAt(row, 0).vertex.z() += 4.0;
    }
    patchOf(patchB).controlPointsChanged();

    Node_setSelected(patchA, true);
    Node_setSelected(patchB, true);
    Node_setSelected(patchC, true);

    GlobalCommandSystem().executeCommand("SewSelectedPatches");

    for (std::size_t row = 0; row < 3; ++row)
    {
        EXPECT_NEAR(patchOf(patchA).ctrlAt(row, 2).vertex.z(), 2.0, 0.001);
        EXPECT_NEAR(patchOf(patchB).ctrlAt(row, 0).vertex.z(), 2.0, 0.001);
    }
}

TEST_F(PatchSewingTest, SewIsUndoable)
{
    auto worldspawn = GlobalMapModule().findOrInsertWorldspawn();

    auto patchA = createPatch(worldspawn, 3, 3, 0.0, 0.0, 32.0, 32.0);
    auto patchB = createPatch(worldspawn, 3, 3, 64.0, 0.0, 32.0, 32.0);

    for (std::size_t row = 0; row < 3; ++row)
    {
        patchOf(patchB).ctrlAt(row, 0).vertex.z() += 4.0;
    }
    patchOf(patchB).controlPointsChanged();

    const Vector3 originalA = patchOf(patchA).ctrlAt(0, 2).vertex;
    const Vector3 originalB = patchOf(patchB).ctrlAt(0, 0).vertex;

    Node_setSelected(patchA, true);
    Node_setSelected(patchB, true);

    GlobalCommandSystem().executeCommand("SewSelectedPatches");

    EXPECT_NEAR(patchOf(patchA).ctrlAt(0, 2).vertex.z(), 2.0, 0.001);

    GlobalCommandSystem().executeCommand("Undo");

    EXPECT_TRUE(math::isNear(patchOf(patchA).ctrlAt(0, 2).vertex, originalA, 0.001));
    EXPECT_TRUE(math::isNear(patchOf(patchB).ctrlAt(0, 0).vertex, originalB, 0.001));
}

TEST_F(PatchSewingTest, SewRejectsPatchesWithDifferentParents)
{
    auto worldspawn = GlobalMapModule().findOrInsertWorldspawn();

    auto funcStaticClass = GlobalEntityClassManager().findClass("func_static");
    auto funcStaticNode = GlobalEntityModule().createEntity(funcStaticClass);
    scene::addNodeToContainer(funcStaticNode, GlobalMapModule().getRoot());

    auto patchA = createPatch(worldspawn, 3, 3, 0.0, 0.0, 32.0, 32.0);
    auto patchB = createPatch(funcStaticNode, 3, 3, 64.0, 0.0, 32.0, 32.0);

    for (std::size_t row = 0; row < 3; ++row)
    {
        patchOf(patchB).ctrlAt(row, 0).vertex.z() += 4.0;
    }
    patchOf(patchB).controlPointsChanged();

    const Vector3 originalA = patchOf(patchA).ctrlAt(0, 2).vertex;
    const Vector3 originalB = patchOf(patchB).ctrlAt(0, 0).vertex;

    Node_setSelected(patchA, true);
    Node_setSelected(patchB, true);

    GlobalCommandSystem().executeCommand("SewSelectedPatches");

    EXPECT_TRUE(math::isNear(patchOf(patchA).ctrlAt(0, 2).vertex, originalA, 0.001));
    EXPECT_TRUE(math::isNear(patchOf(patchB).ctrlAt(0, 0).vertex, originalB, 0.001));
}

}
