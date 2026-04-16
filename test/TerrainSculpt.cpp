#include "RadiantTest.h"

#include "icommandsystem.h"
#include "imap.h"
#include "ipatch.h"
#include "iundo.h"

#include "algorithm/Scene.h"
#include "algorithm/Primitives.h"
#include "selection/TerrainSculptTool.h"

namespace test
{

using TerrainSculptTest = RadiantTest;

namespace
{

const std::string TestMaterial = "textures/common/caulk";

scene::INodePtr createFlatTerrainPatch(int columns, int rows, float spacing, float baseZ = 0.0f)
{
    auto worldspawn = GlobalMapModule().findOrInsertWorldspawn();
    auto node = GlobalPatchModule().createPatch(patch::PatchDefType::Def2);
    worldspawn->addChildNode(node);

    auto* patch = Node_getIPatch(node);
    patch->setDims(columns, rows);
    patch->setShader(TestMaterial);

    for (std::size_t row = 0; row < static_cast<std::size_t>(rows); ++row)
    for (std::size_t col = 0; col < static_cast<std::size_t>(columns); ++col)
    {
        PatchControl& ctrl = patch->ctrlAt(row, col);
        ctrl.vertex.x() = col * spacing;
        ctrl.vertex.y() = row * spacing;
        ctrl.vertex.z() = baseZ;
        ctrl.texcoord.x() = static_cast<float>(col) / (columns - 1);
        ctrl.texcoord.y() = static_cast<float>(row) / (rows - 1);
    }

    patch->controlPointsChanged();
    return node;
}

ui::TerrainSculptSettings makeSettings(float radius, float strength, float falloff = 0.0f,
    ui::TerrainBrushFalloff falloffType = ui::TerrainBrushFalloff::Smooth)
{
    ui::TerrainSculptSettings s;
    s.radius = radius;
    s.strength = strength;
    s.falloff = falloff;
    s.falloffType = falloffType;
    return s;
}

double sumZ(IPatch* patch)
{
    double total = 0.0;
    for (std::size_t row = 0; row < patch->getHeight(); ++row)
    for (std::size_t col = 0; col < patch->getWidth(); ++col)
    {
        total += patch->ctrlAt(row, col).vertex.z();
    }
    return total;
}

double maxZ(IPatch* patch)
{
    double m = -1e18;
    for (std::size_t row = 0; row < patch->getHeight(); ++row)
    for (std::size_t col = 0; col < patch->getWidth(); ++col)
    {
        double z = patch->ctrlAt(row, col).vertex.z();
        if (z > m) m = z;
    }
    return m;
}

double minZ(IPatch* patch)
{
    double m = 1e18;
    for (std::size_t row = 0; row < patch->getHeight(); ++row)
    for (std::size_t col = 0; col < patch->getWidth(); ++col)
    {
        double z = patch->ctrlAt(row, col).vertex.z();
        if (z < m) m = z;
    }
    return m;
}

}

TEST(TerrainSculptBrushWeight, FullWeightInsideFalloffBoundary)
{
    EXPECT_DOUBLE_EQ(ui::terrainSculpt::computeBrushWeight(0.0, 0.5f, ui::TerrainBrushFalloff::Smooth), 1.0);
    EXPECT_DOUBLE_EQ(ui::terrainSculpt::computeBrushWeight(0.3, 0.5f, ui::TerrainBrushFalloff::Linear), 1.0);
    EXPECT_DOUBLE_EQ(ui::terrainSculpt::computeBrushWeight(0.49, 0.5f, ui::TerrainBrushFalloff::Spherical), 1.0);
}

TEST(TerrainSculptBrushWeight, ZeroWeightOutsideBrush)
{
    EXPECT_DOUBLE_EQ(ui::terrainSculpt::computeBrushWeight(1.0, 0.5f, ui::TerrainBrushFalloff::Smooth), 0.0);
    EXPECT_DOUBLE_EQ(ui::terrainSculpt::computeBrushWeight(1.5, 0.0f, ui::TerrainBrushFalloff::Linear), 0.0);
}

TEST(TerrainSculptBrushWeight, HardEdgeWithZeroFalloff)
{
    EXPECT_DOUBLE_EQ(ui::terrainSculpt::computeBrushWeight(0.0, 0.0f, ui::TerrainBrushFalloff::Linear), 1.0);
    EXPECT_DOUBLE_EQ(ui::terrainSculpt::computeBrushWeight(0.5, 0.0f, ui::TerrainBrushFalloff::Linear), 1.0);
    EXPECT_DOUBLE_EQ(ui::terrainSculpt::computeBrushWeight(0.99, 0.0f, ui::TerrainBrushFalloff::Linear), 1.0);
}

TEST(TerrainSculptBrushWeight, LinearFalloffMidpoint)
{
    double w = ui::terrainSculpt::computeBrushWeight(0.75, 1.0f, ui::TerrainBrushFalloff::Linear);
    EXPECT_NEAR(w, 0.25, 0.001);
}

TEST(TerrainSculptBrushWeight, SmoothFalloffSymmetry)
{
    double center = ui::terrainSculpt::computeBrushWeight(0.5, 1.0f, ui::TerrainBrushFalloff::Smooth);
    EXPECT_NEAR(center, 0.5, 0.001);
}

TEST(TerrainSculptBrushWeight, SphericalStartsSmoothEndSharp)
{
    double early = ui::terrainSculpt::computeBrushWeight(0.1, 1.0f, ui::TerrainBrushFalloff::Spherical);
    double late  = ui::terrainSculpt::computeBrushWeight(0.9, 1.0f, ui::TerrainBrushFalloff::Spherical);
    EXPECT_GT(early, 0.9);
    EXPECT_LT(late, 0.5);
}

TEST(TerrainSculptBrushWeight, TipStartsSharpEndSmooth)
{
    double early = ui::terrainSculpt::computeBrushWeight(0.1, 1.0f, ui::TerrainBrushFalloff::Tip);
    double mid   = ui::terrainSculpt::computeBrushWeight(0.5, 1.0f, ui::TerrainBrushFalloff::Tip);
    double late  = ui::terrainSculpt::computeBrushWeight(0.9, 1.0f, ui::TerrainBrushFalloff::Tip);
    EXPECT_GT(early, mid) << "Weight should increase toward center";
    EXPECT_GT(mid, late);
    EXPECT_GT(late, 0.0);
    EXPECT_LT(early, 0.9) << "Tip drops sharply near the center";
}

TEST_F(TerrainSculptTest, RaiseIncreasesHeight)
{
    auto node = createFlatTerrainPatch(5, 5, 32.0f, 0.0f);
    auto* patch = Node_getIPatch(node);

    auto s = makeSettings(256.0f, 8.0f);
    Vector3 center(64.0, 64.0, 0.0);

    ui::terrainSculpt::applyRaiseLower(patch, center, s, +1.0f);
    patch->controlPointsChanged();

    EXPECT_GT(sumZ(patch), 0.0) << "Raise should increase total Z";
    EXPECT_GT(maxZ(patch), 0.0) << "At least one vertex should have risen";
}

TEST_F(TerrainSculptTest, LowerDecreasesHeight)
{
    auto node = createFlatTerrainPatch(5, 5, 32.0f, 100.0f);
    auto* patch = Node_getIPatch(node);

    auto s = makeSettings(256.0f, 8.0f);
    Vector3 center(64.0, 64.0, 100.0);

    ui::terrainSculpt::applyRaiseLower(patch, center, s, -1.0f);
    patch->controlPointsChanged();

    EXPECT_LT(minZ(patch), 100.0) << "Lower should decrease at least one vertex";
}

TEST_F(TerrainSculptTest, RaiseRespectsRadius)
{
    auto node = createFlatTerrainPatch(5, 5, 64.0f, 0.0f);
    auto* patch = Node_getIPatch(node);

    auto s = makeSettings(32.0f, 8.0f);
    Vector3 center(0.0, 0.0, 0.0);

    ui::terrainSculpt::applyRaiseLower(patch, center, s, +1.0f);

    EXPECT_GT(patch->ctrlAt(0, 0).vertex.z(), 0.0) << "Origin vertex should be raised";
    EXPECT_DOUBLE_EQ(patch->ctrlAt(4, 4).vertex.z(), 0.0) << "Distant vertex should be untouched";
}

TEST_F(TerrainSculptTest, FlattenConvergesToTarget)
{
    auto node = createFlatTerrainPatch(5, 5, 32.0f, 0.0f);
    auto* patch = Node_getIPatch(node);

    patch->ctrlAt(0, 0).vertex.z() = 10.0;
    patch->ctrlAt(1, 1).vertex.z() = -5.0;
    patch->ctrlAt(2, 2).vertex.z() = 20.0;

    auto s = makeSettings(512.0f, 100.0f);
    Vector3 center(64.0, 64.0, 0.0);

    for (int i = 0; i < 50; ++i)
    {
        ui::terrainSculpt::applyFlatten(patch, center, 0.0, s);
    }

    for (std::size_t row = 0; row < patch->getHeight(); ++row)
    for (std::size_t col = 0; col < patch->getWidth(); ++col)
    {
        EXPECT_NEAR(patch->ctrlAt(row, col).vertex.z(), 0.0, 0.1)
            << "All vertices should converge to target height after repeated flattening";
    }
}

TEST_F(TerrainSculptTest, SmoothReducesVariance)
{
    auto node = createFlatTerrainPatch(5, 5, 32.0f, 0.0f);
    auto* patch = Node_getIPatch(node);

    patch->ctrlAt(2, 2).vertex.z() = 100.0;

    double rangeBefore = maxZ(patch) - minZ(patch);

    auto s = makeSettings(512.0f, 50.0f);
    s.smoothFilterRadius = 2.0f;
    Vector3 center(64.0, 64.0, 0.0);

    for (int i = 0; i < 20; ++i)
    {
        ui::terrainSculpt::applySmooth(patch, center, s);
    }

    double rangeAfter = maxZ(patch) - minZ(patch);
    EXPECT_LT(rangeAfter, rangeBefore) << "Smoothing should reduce height range";
}

TEST_F(TerrainSculptTest, NoiseAddsVariation)
{
    auto node = createFlatTerrainPatch(5, 5, 32.0f, 0.0f);
    auto* patch = Node_getIPatch(node);

    auto s = makeSettings(512.0f, 100.0f);
    s.noiseAlgorithm = noise::Algorithm::Perlin;
    s.noiseScale = 0.05f;
    s.noiseAmount = 16.0f;
    s.noiseSeed = 42;
    Vector3 center(64.0, 64.0, 0.0);

    ui::terrainSculpt::applyNoise(patch, center, s);

    double range = maxZ(patch) - minZ(patch);
    EXPECT_GT(range, 0.0) << "Noise should create height variation on a flat patch";
}

TEST_F(TerrainSculptTest, NoiseSeedProducesDifferentResults)
{
    auto node1 = createFlatTerrainPatch(5, 5, 32.0f, 0.0f);
    auto* patch1 = Node_getIPatch(node1);
    auto node2 = createFlatTerrainPatch(5, 5, 32.0f, 0.0f);
    auto* patch2 = Node_getIPatch(node2);

    auto s1 = makeSettings(512.0f, 100.0f);
    s1.noiseAlgorithm = noise::Algorithm::Perlin;
    s1.noiseScale = 0.05f;
    s1.noiseAmount = 16.0f;
    s1.noiseSeed = 0;
    Vector3 center(64.0, 64.0, 0.0);

    auto s2 = s1;
    s2.noiseSeed = 999;

    ui::terrainSculpt::applyNoise(patch1, center, s1);
    ui::terrainSculpt::applyNoise(patch2, center, s2);

    bool anyDiff = false;
    for (std::size_t row = 0; row < patch1->getHeight() && !anyDiff; ++row)
    for (std::size_t col = 0; col < patch1->getWidth() && !anyDiff; ++col)
    {
        if (std::abs(patch1->ctrlAt(row, col).vertex.z() - patch2->ctrlAt(row, col).vertex.z()) > 0.001)
        {
            anyDiff = true;
        }
    }
    EXPECT_TRUE(anyDiff) << "Different seeds should produce different noise patterns";
}

TEST_F(TerrainSculptTest, OperationsAreUndoable)
{
    auto node = createFlatTerrainPatch(5, 5, 32.0f, 0.0f);
    auto* patch = Node_getIPatch(node);

    auto s = makeSettings(256.0f, 8.0f);
    Vector3 center(64.0, 64.0, 0.0);

    {
        UndoableCommand undo("terrainSculptTest");
        patch->undoSave();
        ui::terrainSculpt::applyRaiseLower(patch, center, s, +1.0f);
        patch->controlPointsChanged();
    }

    EXPECT_GT(sumZ(patch), 0.0);

    GlobalUndoSystem().undo();

    EXPECT_NEAR(sumZ(patch), 0.0, 0.001) << "Undo should restore original flat state";
}

TEST_F(TerrainSculptTest, OperationsMissWhenCenterOutsidePatch)
{
    auto node = createFlatTerrainPatch(5, 5, 32.0f, 0.0f);
    auto* patch = Node_getIPatch(node);

    auto s = makeSettings(32.0f, 8.0f);
    Vector3 farAway(10000.0, 10000.0, 0.0);

    bool touched = ui::terrainSculpt::applyRaiseLower(patch, farAway, s, +1.0f);

    EXPECT_FALSE(touched) << "Should not touch any vertices when center is far from patch";
    EXPECT_NEAR(sumZ(patch), 0.0, 0.001);
}

} // namespace test
