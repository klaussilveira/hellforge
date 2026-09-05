#include "RadiantTest.h"

#include "imodelcache.h"
#include "model/BestViewSolver.h"

namespace test
{

using ThumbnailViewTest = RadiantTest;

namespace
{

double areaAtDefaultView(const model::IModel& model)
{
    return model::calculateVisibleArea(model,
        model::getViewDirection(model::getDefaultViewAngles()));
}

double areaAtSolvedView(const model::IModel& model)
{
    return model::calculateVisibleArea(model,
        model::getViewDirection(model::calculateBestViewAngles(model)));
}

}

TEST_F(ThumbnailViewTest, ViewAnglesRoundTripToDirection)
{
    auto angles = model::getDefaultViewAngles();

    EXPECT_NEAR(angles[0], 34, 0.001);
    EXPECT_NEAR(angles[1], 135, 0.001);

    auto direction = model::getViewDirection(angles);
    auto expected = model::getViewDirection(model::DefaultViewElevation, model::DefaultViewAzimuth);

    EXPECT_NEAR(direction.x(), expected.x(), 0.001);
    EXPECT_NEAR(direction.y(), expected.y(), 0.001);
    EXPECT_NEAR(direction.z(), expected.z(), 0.001);
}

TEST_F(ThumbnailViewTest, SolvedViewIsNeverWorseThanDefault)
{
    const char* const modelPaths[] =
    {
        "models/ase/testcube.ase",
        "models/ase/testsphere.ase",
        "models/ase/tiles.ase",
        "models/ase/single_triangle.ase",
        "models/moss_patch.ase",
        "models/torch.lwo",
    };

    for (const auto* modelPath : modelPaths)
    {
        auto model = GlobalModelCache().getModel(modelPath);
        ASSERT_TRUE(model) << modelPath;

        EXPECT_GE(areaAtSolvedView(*model), areaAtDefaultView(*model)) << modelPath;
    }
}

TEST_F(ThumbnailViewTest, SingleSidedPlaneIsFacedHeadOn)
{
    auto model = GlobalModelCache().getModel("models/ase/single_triangle.ase");
    ASSERT_TRUE(model);

    EXPECT_GT(areaAtSolvedView(*model), 0);
    EXPECT_GT(areaAtSolvedView(*model), areaAtDefaultView(*model));

    auto angles = model::calculateBestViewAngles(*model);

    EXPECT_GT(angles[0], model::DefaultViewElevation);
}

TEST_F(ThumbnailViewTest, SymmetricModelKeepsTheDefaultView)
{
    auto model = GlobalModelCache().getModel("models/ase/testcube.ase");
    ASSERT_TRUE(model);

    auto angles = model::calculateBestViewAngles(*model);

    EXPECT_NEAR(angles[0], model::DefaultViewElevation, 0.001);
    EXPECT_NEAR(angles[1], 135, 0.001);
}

}
