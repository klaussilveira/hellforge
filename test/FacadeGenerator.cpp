#include "RadiantTest.h"

#include "ibrush.h"
#include "ientity.h"
#include "imap.h"
#include "iscenegraph.h"
#include "scene/Entity.h"
#include "scenelib.h"
#include "math/AABB.h"
#include "math/Vector3.h"

#include "ui/facade/FacadeGeometry.h"

#include <algorithm>
#include <cmath>
#include <set>
#include <string>

namespace test
{
using FacadeGeneratorTest = RadiantTest;

namespace
{
const double WALL_LENGTH = 480;
const double WALL_TOP = 400;
const double THICKNESS = 20;

struct Box
{
    Vector3 mins;
    Vector3 maxs;
};

facade::FacadePath straightPath(double length = WALL_LENGTH, double topZ = WALL_TOP)
{
    facade::FacadePath path;
    path.points.push_back(Vector3(0, 0, 0));
    path.points.push_back(Vector3(length, 0, 0));
    path.baseZ = 0;
    path.topZ = topZ;
    path.bodyDepth = 0;

    return path;
}

facade::FacadePath bentPath()
{
    facade::FacadePath path;
    path.points.push_back(Vector3(0, 0, 0));
    path.points.push_back(Vector3(240, 0, 0));
    path.points.push_back(Vector3(240, 240, 0));
    path.baseZ = 0;
    path.topZ = WALL_TOP;
    path.bodyDepth = 0;

    return path;
}

facade::FacadeParams plainParams()
{
    facade::FacadeParams params = facade::getPreset(facade::PRESET_BLANK);

    params.fitToSource = false;
    params.floorCount = 2;
    params.wallThickness = THICKNESS;
    params.minEndPier = 20;

    params.ground.height = 160;
    params.ground.frontOffset = 0;
    params.ground.bayPitch = 160;
    params.ground.openingWidth = 80;
    params.ground.openingHeight = 100;
    params.ground.sillHeight = 20;

    params.upper.height = 120;
    params.upper.frontOffset = 0;
    params.upper.bayPitch = 120;
    params.upper.openingWidth = 40;
    params.upper.openingHeight = 80;
    params.upper.sillHeight = 20;

    params.groundDoor = false;
    params.arcadeColumns = false;
    params.solidBody = false;

    params.plinthHeight = 0;
    params.trimProud = 0;
    params.courseHeight = 0;
    params.courseProud = 0;
    params.corniceHeight = 0;
    params.corniceProud = 0;
    params.parapetHeight = 0;

    params.tileUnits = 0;

    return params;
}

std::vector<Box> boxesOf(const std::vector<scene::INodePtr>& nodes)
{
    std::vector<Box> boxes;

    for (const scene::INodePtr& node : nodes)
    {
        AABB bounds = node->worldAABB();
        boxes.push_back({bounds.origin - bounds.extents, bounds.origin + bounds.extents});
    }

    return boxes;
}

double volumeOf(const Box& box)
{
    return (box.maxs.x() - box.mins.x()) * (box.maxs.y() - box.mins.y()) *
           (box.maxs.z() - box.mins.z());
}

double overlapVolume(const Box& a, const Box& b)
{
    double volume = 1;

    for (int axis = 0; axis < 3; ++axis)
    {
        double low = std::max(a.mins[axis], b.mins[axis]);
        double high = std::min(a.maxs[axis], b.maxs[axis]);

        if (high - low <= 0.01)
        {
            return 0;
        }

        volume *= (high - low);
    }

    return volume;
}

bool containsPoint(const std::vector<Box>& boxes, const Vector3& point)
{
    for (const Box& box : boxes)
    {
        bool inside = true;

        for (int axis = 0; axis < 3; ++axis)
        {
            if (point[axis] <= box.mins[axis] + 0.01 || point[axis] >= box.maxs[axis] - 0.01)
            {
                inside = false;
                break;
            }
        }

        if (inside)
        {
            return true;
        }
    }

    return false;
}

struct PlanBox
{
    double a0, a1, d0, d1, z0, z1;
};

std::vector<PlanBox> planBoxes(const std::vector<facade::WallSpan>& spans)
{
    std::vector<PlanBox> boxes;

    for (const facade::WallSpan& span : spans)
    {
        boxes.push_back({span.a0, span.a1, span.back, span.front, span.z0, span.z1});
    }

    return boxes;
}

double planOverlap(const PlanBox& a, const PlanBox& b)
{
    double volume = 1;
    double pairs[3][4] = {{a.a0, a.a1, b.a0, b.a1},
                          {a.d0, a.d1, b.d0, b.d1},
                          {a.z0, a.z1, b.z0, b.z1}};

    for (int axis = 0; axis < 3; ++axis)
    {
        double low = std::max(pairs[axis][0], pairs[axis][2]);
        double high = std::min(pairs[axis][1], pairs[axis][3]);

        if (high - low <= 0.01)
        {
            return 0;
        }

        volume *= (high - low);
    }

    return volume;
}

int countRole(const std::vector<facade::WallSpan>& spans, int role)
{
    int count = 0;

    for (const facade::WallSpan& span : spans)
    {
        if (span.role == role)
        {
            ++count;
        }
    }

    return count;
}

const facade::WallSpan* findRole(const std::vector<facade::WallSpan>& spans, int role)
{
    for (const facade::WallSpan& span : spans)
    {
        if (span.role == role)
        {
            return &span;
        }
    }

    return nullptr;
}

facade::BandStyle ribbonStyle()
{
    facade::BandStyle style;
    style.height = 120;
    style.bayPitch = 120;
    style.openingWidth = 100;
    style.openingHeight = 60;
    style.sillHeight = 40;

    return style;
}

} // anonymous namespace

TEST_F(FacadeGeneratorTest, BaysLeaveAnEvenlySplitPierAtBothEnds)
{
    facade::BandStyle style;
    style.bayPitch = 160;
    style.openingWidth = 80;
    style.openingHeight = 100;
    style.sillHeight = 20;

    auto openings = facade::layoutOpenings(480, 0, style, 20, false, 0, 0);

    ASSERT_EQ(openings.size(), 2);
    EXPECT_NEAR(openings[0].a0, 120, 0.01);
    EXPECT_NEAR(openings[0].a1, 200, 0.01);
    EXPECT_NEAR(openings[1].a0, 280, 0.01);
    EXPECT_NEAR(openings[1].a1, 360, 0.01);
}

TEST_F(FacadeGeneratorTest, BayPitchIsHeldConstantWhenTheLeftoverIsNotEven)
{
    facade::BandStyle style;
    style.bayPitch = 120;
    style.openingWidth = 60;
    style.openingHeight = 80;
    style.sillHeight = 20;

    auto openings = facade::layoutOpenings(500, 0, style, 20, false, 0, 0);

    ASSERT_EQ(openings.size(), 3);
    EXPECT_NEAR(openings[0].a1 - openings[0].a0, 60, 0.01);
    EXPECT_NEAR(openings[1].a0 - openings[0].a1, 60, 0.01);
    EXPECT_NEAR(openings[2].a0 - openings[1].a1, 60, 0.01);
    EXPECT_NEAR(openings[0].a0, 120, 0.01);
    EXPECT_NEAR(500 - openings[2].a1, 80, 0.01);
}

TEST_F(FacadeGeneratorTest, AnExplicitBayCountGivesExactlyThatManyWindows)
{
    facade::BandStyle style;
    style.bayPitch = 160;
    style.openingWidth = 80;
    style.openingHeight = 100;
    style.sillHeight = 20;

    for (int requested = 1; requested <= 4; ++requested)
    {
        style.bayCount = requested;

        auto openings = facade::layoutOpenings(480, 0, style, 20, false, 0, 0);

        EXPECT_EQ(static_cast<int>(openings.size()), requested)
            << "asked for " << requested << " bays";

        for (const facade::Opening& opening : openings)
        {
            EXPECT_NEAR(opening.a1 - opening.a0, 80, 0.01);
        }
    }
}

TEST_F(FacadeGeneratorTest, AnExplicitBayCountOverridesThePitch)
{
    facade::BandStyle style;
    style.bayPitch = 160;
    style.openingWidth = 80;
    style.openingHeight = 100;
    style.sillHeight = 20;

    ASSERT_EQ(facade::layoutOpenings(480, 0, style, 20, false, 0, 0).size(), 2);

    style.bayCount = 3;

    auto openings = facade::layoutOpenings(480, 0, style, 20, false, 0, 0);

    ASSERT_EQ(openings.size(), 3);
    EXPECT_NEAR(openings[0].a0, 60, 0.01);
    EXPECT_NEAR(openings[1].a0, 200, 0.01);
    EXPECT_NEAR(openings[2].a0, 340, 0.01);
}

TEST_F(FacadeGeneratorTest, ABayCountOfZeroStillUsesThePitch)
{
    facade::BandStyle style;
    style.bayCount = 0;
    style.bayPitch = 120;
    style.openingWidth = 60;
    style.openingHeight = 80;
    style.sillHeight = 20;

    auto openings = facade::layoutOpenings(480, 0, style, 20, false, 0, 0);

    ASSERT_EQ(openings.size(), 3);
    EXPECT_NEAR(openings[0].a0, 100, 0.01);
    EXPECT_NEAR(openings[0].a1, 160, 0.01);
}

TEST_F(FacadeGeneratorTest, AnExcessiveBayCountIsClampedRatherThanMergingWindows)
{
    facade::BandStyle style;
    style.bayPitch = 160;
    style.openingWidth = 80;
    style.openingHeight = 100;
    style.sillHeight = 20;
    style.bayCount = 8;

    auto openings = facade::layoutOpenings(480, 0, style, 20, false, 0, 0);

    ASSERT_EQ(openings.size(), 4);

    for (std::size_t i = 1; i < openings.size(); ++i)
    {
        EXPECT_GE(openings[i].a0 - openings[i - 1].a1, facade::GRID - 0.01)
            << "pier " << i << " collapsed";
    }
}

TEST_F(FacadeGeneratorTest, ExplicitBaysStayOnTheGridAndOffTheCorners)
{
    facade::BandStyle style;
    style.openingWidth = 60;
    style.openingHeight = 80;
    style.sillHeight = 20;
    style.bayPitch = 120;

    int checked = 0;

    for (double length = 200; length <= 1200; length += 10)
    {
        for (int requested = 1; requested <= 10; ++requested)
        {
            style.bayCount = requested;

            auto openings = facade::layoutOpenings(length, 0, style, 20, false, 0, 0);

            for (const facade::Opening& opening : openings)
            {
                ++checked;
                EXPECT_NEAR(opening.a0, facade::snapTo(opening.a0, facade::GRID), 0.01)
                    << "length " << length << " count " << requested;
                EXPECT_GE(opening.a0, 20 - 0.01);
                EXPECT_LE(opening.a1, length - 20 + 0.01);
            }
        }
    }

    EXPECT_GT(checked, 0);
}

TEST_F(FacadeGeneratorTest, TheBayCountControlsTheWindowsActuallyCutIntoTheWall)
{
    auto worldspawn = GlobalMapModule().findOrInsertWorldspawn();

    facade::FacadeParams params = plainParams();
    params.ground.bayCount = 2;

    auto boxes = boxesOf(facade::generateFacade(straightPath(), params, worldspawn));

    ASSERT_FALSE(boxes.empty());

    EXPECT_FALSE(containsPoint(boxes, Vector3(140, -10, 70)));
    EXPECT_FALSE(containsPoint(boxes, Vector3(360, -10, 70)));

    EXPECT_TRUE(containsPoint(boxes, Vector3(250, -10, 70)));
    EXPECT_TRUE(containsPoint(boxes, Vector3(40, -10, 70)));
    EXPECT_TRUE(containsPoint(boxes, Vector3(440, -10, 70)));

    EXPECT_FALSE(containsPoint(boxes, Vector3(120, -10, 220)));
}

TEST_F(FacadeGeneratorTest, AutoBaysStayOnTheGridWhenTheHalfPierWouldNot)
{
    facade::BandStyle style;
    style.bayCount = 0;
    style.openingWidth = 60;
    style.openingHeight = 80;
    style.sillHeight = 20;

    int checked = 0;

    for (double pitch = 100; pitch <= 260; pitch += 20)
    {
        style.bayPitch = pitch;

        for (double length = 200; length <= 1200; length += 10)
        {
            for (const facade::Opening& opening :
                 facade::layoutOpenings(length, 0, style, 20, false, 0, 0))
            {
                ++checked;
                EXPECT_NEAR(opening.a0, facade::snapTo(opening.a0, facade::GRID), 0.01)
                    << "pitch " << pitch << " length " << length;
                EXPECT_NEAR(opening.a1, facade::snapTo(opening.a1, facade::GRID), 0.01)
                    << "pitch " << pitch << " length " << length;
            }
        }
    }

    EXPECT_GT(checked, 0);
}

TEST_F(FacadeGeneratorTest, AWallTooShortForOneBayGetsNoOpenings)
{
    facade::BandStyle style;
    style.bayPitch = 160;
    style.openingWidth = 80;
    style.openingHeight = 100;

    EXPECT_TRUE(facade::layoutOpenings(120, 0, style, 20, false, 0, 0).empty());
}

TEST_F(FacadeGeneratorTest, AnOpeningWiderThanItsBayIsRejected)
{
    facade::BandStyle style;
    style.bayPitch = 120;
    style.openingWidth = 120;
    style.openingHeight = 80;

    EXPECT_TRUE(facade::layoutOpenings(480, 0, style, 20, false, 0, 0).empty());
}

TEST_F(FacadeGeneratorTest, TheDoorReplacesTheMiddleBayAndReachesTheGround)
{
    facade::BandStyle style;
    style.bayPitch = 160;
    style.openingWidth = 80;
    style.openingHeight = 100;
    style.sillHeight = 20;

    auto openings = facade::layoutOpenings(800, 0, style, 20, true, 60, 120);

    ASSERT_EQ(openings.size(), 4);
    EXPECT_NEAR(openings[2].z0, 0, 0.01);
    EXPECT_NEAR(openings[2].z1, 120, 0.01);
    EXPECT_NEAR(openings[2].a1 - openings[2].a0, 60, 0.01);

    EXPECT_NEAR(openings[0].z0, 20, 0.01);
}

TEST_F(FacadeGeneratorTest, FitModeFillsTheSourceHeightExactly)
{
    facade::FacadeParams params = plainParams();
    params.fitToSource = true;
    params.corniceHeight = 60;
    params.parapetHeight = 40;

    auto bands = facade::buildBands(params, 0, WALL_TOP);

    ASSERT_FALSE(bands.empty());
    EXPECT_NEAR(bands.back().z1, WALL_TOP, 0.01);
    EXPECT_NEAR(bands.front().z0, 0, 0.01);
}

TEST_F(FacadeGeneratorTest, FitModeSnapsUpperFloorsToTheHalfMetreGrid)
{
    facade::FacadeParams params = plainParams();
    params.fitToSource = true;
    params.ground.height = 160;
    params.upper.height = 120;

    auto bands = facade::buildBands(params, 0, 745);

    int upperCount = 0;

    for (const facade::Band& band : bands)
    {
        if (band.kind != facade::BAND_UPPER)
        {
            continue;
        }

        ++upperCount;

        double height = band.z1 - band.z0;
        EXPECT_NEAR(height, facade::snapTo(height, facade::GRID), 0.01);
    }

    EXPECT_GT(upperCount, 0);
}

TEST_F(FacadeGeneratorTest, ExplicitFloorCountIgnoresTheSourceHeight)
{
    facade::FacadeParams params = plainParams();
    params.fitToSource = false;
    params.floorCount = 5;

    auto bands = facade::buildBands(params, 0, WALL_TOP);

    int upperCount = 0;

    for (const facade::Band& band : bands)
    {
        if (band.kind == facade::BAND_UPPER)
        {
            ++upperCount;
        }
    }

    EXPECT_EQ(upperCount, 5);
    EXPECT_NEAR(bands.back().z1, 760, 0.01);
}

TEST_F(FacadeGeneratorTest, PanelBrushesAndOpeningsExactlyTileTheWallSlab)
{
    auto worldspawn = GlobalMapModule().findOrInsertWorldspawn();

    auto nodes = facade::generateFacade(straightPath(), plainParams(), worldspawn);

    ASSERT_FALSE(nodes.empty());

    auto boxes = boxesOf(nodes);

    for (std::size_t i = 0; i < boxes.size(); ++i)
    {
        for (std::size_t j = i + 1; j < boxes.size(); ++j)
        {
            EXPECT_NEAR(overlapVolume(boxes[i], boxes[j]), 0, 0.01)
                << "brush " << i << " overlaps brush " << j;
        }
    }

    double solid = 0;

    for (const Box& box : boxes)
    {
        solid += volumeOf(box);
    }

    const double slabVolume = 480.0 * 20.0 * 400.0;
    const double openingVolume = 2 * (80.0 * 20.0 * 100.0) + 6 * (40.0 * 20.0 * 80.0);

    EXPECT_NEAR(solid, slabVolume - openingVolume, 1.0);
}

TEST_F(FacadeGeneratorTest, WindowOpeningsAreLeftEmptyAndPiersAreSolid)
{
    auto worldspawn = GlobalMapModule().findOrInsertWorldspawn();

    auto boxes = boxesOf(facade::generateFacade(straightPath(), plainParams(), worldspawn));

    ASSERT_FALSE(boxes.empty());

    EXPECT_FALSE(containsPoint(boxes, Vector3(160, -10, 70)));

    EXPECT_FALSE(containsPoint(boxes, Vector3(120, -10, 220)));

    EXPECT_TRUE(containsPoint(boxes, Vector3(240, -10, 70)));

    EXPECT_TRUE(containsPoint(boxes, Vector3(160, -10, 140)));

    EXPECT_TRUE(containsPoint(boxes, Vector3(160, -10, 10)));
}

TEST_F(FacadeGeneratorTest, UpperFloorsCantileverForwardOfTheGroundFloor)
{
    auto worldspawn = GlobalMapModule().findOrInsertWorldspawn();

    facade::FacadeParams params = plainParams();
    params.upper.frontOffset = 40;

    auto boxes = boxesOf(facade::generateFacade(straightPath(), params, worldspawn));

    ASSERT_FALSE(boxes.empty());

    double groundFront = -1000;
    double upperFront = -1000;

    for (const Box& box : boxes)
    {
        if (box.maxs.z() <= 160.01)
        {
            groundFront = std::max(groundFront, box.maxs.y());
        }
        else if (box.mins.z() >= 159.99)
        {
            upperFront = std::max(upperFront, box.maxs.y());
        }
    }

    EXPECT_NEAR(groundFront, 0, 0.01);
    EXPECT_NEAR(upperFront, 40, 0.01);

    double back = 1000;

    for (const Box& box : boxes)
    {
        back = std::min(back, box.mins.y());
    }

    EXPECT_NEAR(back, -THICKNESS, 0.01);
}

TEST_F(FacadeGeneratorTest, ArcadeColumnsOnlyFillUnderThePiers)
{
    auto worldspawn = GlobalMapModule().findOrInsertWorldspawn();

    facade::FacadeParams params = plainParams();
    params.upper.frontOffset = 40;
    params.arcadeColumns = true;

    auto boxes = boxesOf(facade::generateFacade(straightPath(), params, worldspawn));

    EXPECT_TRUE(containsPoint(boxes, Vector3(240, 20, 80)));

    EXPECT_FALSE(containsPoint(boxes, Vector3(160, 20, 80)));
}

TEST_F(FacadeGeneratorTest, EveryDimensionIsAWholeNumberOfTiles)
{
    auto worldspawn = GlobalMapModule().findOrInsertWorldspawn();

    auto boxes = boxesOf(facade::generateFacade(straightPath(), plainParams(), worldspawn));

    ASSERT_FALSE(boxes.empty());

    const double tile = facade::GRID;

    for (const Box& box : boxes)
    {
        for (int axis = 0; axis < 3; ++axis)
        {
            double size = box.maxs[axis] - box.mins[axis];
            EXPECT_NEAR(size, facade::snapTo(size, tile), 0.01)
                << "axis " << axis << " size " << size << " is off the half-metre grid";
        }
    }
}

TEST_F(FacadeGeneratorTest, ACompressedFloorStillKeepsAHeaderAboveTheWindow)
{
    auto worldspawn = GlobalMapModule().findOrInsertWorldspawn();

    facade::FacadeParams params = plainParams();
    params.fitToSource = true;
    params.upper.height = 120;
    params.upper.sillHeight = 20;
    params.upper.openingHeight = 100;

    facade::FacadePath path = straightPath(WALL_LENGTH, 560);

    auto boxes = boxesOf(facade::generateFacade(path, params, worldspawn));

    ASSERT_FALSE(boxes.empty());

    EXPECT_TRUE(containsPoint(boxes, Vector3(120, -10, 270)));

    EXPECT_FALSE(containsPoint(boxes, Vector3(120, -10, 200)));
}

TEST_F(FacadeGeneratorTest, NoPresetProducesOverlappingBrushes)
{
    auto worldspawn = GlobalMapModule().findOrInsertWorldspawn();

    const int presets[] = {facade::PRESET_BLANK, facade::PRESET_HONGKONG,
                           facade::PRESET_NEWYORK, facade::PRESET_CURTAIN,
                           facade::PRESET_BRUTALIST, facade::PRESET_WAREHOUSE,
                           facade::PRESET_RIBBON};

    for (int preset : presets)
    {
        facade::FacadeParams params = facade::getPreset(preset);
        params.fitToSource = true;
        params.solidBody = true;

        facade::FacadePath path = straightPath(800, 720);
        path.bodyDepth = 300;

        auto nodes = facade::generateFacade(path, params, worldspawn);

        ASSERT_FALSE(nodes.empty()) << "preset " << preset << " generated nothing";

        auto boxes = boxesOf(nodes);

        for (std::size_t i = 0; i < boxes.size(); ++i)
        {
            for (std::size_t j = i + 1; j < boxes.size(); ++j)
            {
                EXPECT_NEAR(overlapVolume(boxes[i], boxes[j]), 0, 0.01)
                    << "preset " << preset << ": brush " << i << " overlaps brush " << j;
            }
        }

        for (const scene::INodePtr& node : nodes)
        {
            scene::removeNodeFromParent(node);
        }
    }
}

TEST_F(FacadeGeneratorTest, SillsClearTheFloorCourseTheySitOn)
{
    auto worldspawn = GlobalMapModule().findOrInsertWorldspawn();

    facade::FacadeParams params = plainParams();
    params.floorCount = 1;
    params.upper.sillHeight = 40;
    params.trimProud = 20;
    params.courseHeight = 20;
    params.courseProud = 20;

    auto boxes = boxesOf(facade::generateFacade(straightPath(), params, worldspawn));

    for (std::size_t i = 0; i < boxes.size(); ++i)
    {
        for (std::size_t j = i + 1; j < boxes.size(); ++j)
        {
            EXPECT_NEAR(overlapVolume(boxes[i], boxes[j]), 0, 0.01);
        }
    }

    EXPECT_TRUE(containsPoint(boxes, Vector3(240, 10, 170)));
    EXPECT_TRUE(containsPoint(boxes, Vector3(120, 10, 190)));
}

TEST_F(FacadeGeneratorTest, TrimDoesNotReachSidewaysIntoAnArcadeColumn)
{
    auto worldspawn = GlobalMapModule().findOrInsertWorldspawn();

    facade::FacadeParams params = plainParams();
    params.upper.frontOffset = 40;
    params.arcadeColumns = true;
    params.trimProud = 20;

    auto boxes = boxesOf(facade::generateFacade(straightPath(), params, worldspawn));

    for (std::size_t i = 0; i < boxes.size(); ++i)
    {
        for (std::size_t j = i + 1; j < boxes.size(); ++j)
        {
            EXPECT_NEAR(overlapVolume(boxes[i], boxes[j]), 0, 0.01)
                << "brush " << i << " overlaps brush " << j;
        }
    }
}

TEST_F(FacadeGeneratorTest, CorniceStepsMarchOutInWholeGridIncrements)
{
    auto worldspawn = GlobalMapModule().findOrInsertWorldspawn();

    facade::FacadeParams params = plainParams();
    params.floorCount = 1;
    params.corniceHeight = 60;
    params.corniceProud = 40;
    params.corniceSteps = 3;

    auto boxes = boxesOf(facade::generateFacade(straightPath(), params, worldspawn));

    std::vector<double> fronts;

    for (const Box& box : boxes)
    {
        if (box.mins.z() >= 279.99 && box.maxs.z() <= 340.01)
        {
            fronts.push_back(box.maxs.y());
        }
    }

    ASSERT_EQ(fronts.size(), 3);
    std::sort(fronts.begin(), fronts.end());

    EXPECT_NEAR(fronts[0], 0, 0.01);
    EXPECT_NEAR(fronts[1], 20, 0.01);
    EXPECT_NEAR(fronts[2], 40, 0.01);
}

TEST_F(FacadeGeneratorTest, TrimGoesIntoAFuncStaticCarryingTheKeysDmapNeeds)
{
    auto worldspawn = GlobalMapModule().findOrInsertWorldspawn();

    facade::FacadeParams params = facade::getPreset(facade::PRESET_NEWYORK);
    params.trimAsEntity = true;
    params.solidBody = false;

    auto nodes = facade::generateFacade(straightPath(800, 720), params, worldspawn);

    ASSERT_FALSE(nodes.empty());

    std::set<scene::INode*> trimParents;
    int worldBrushes = 0;

    for (const scene::INodePtr& node : nodes)
    {
        scene::INodePtr nodeParent = node->getParent();
        ASSERT_TRUE(nodeParent != nullptr);

        if (nodeParent == worldspawn)
        {
            ++worldBrushes;
        }
        else
        {
            trimParents.insert(nodeParent.get());
        }
    }

    EXPECT_GT(worldBrushes, 0);

    ASSERT_EQ(trimParents.size(), 1);

    scene::INodePtr trimEntityNode;

    for (const scene::INodePtr& node : nodes)
    {
        if (node->getParent() != worldspawn)
        {
            trimEntityNode = node->getParent();
            break;
        }
    }

    ASSERT_TRUE(trimEntityNode != nullptr);

    Entity* entity = trimEntityNode->tryGetEntity();
    ASSERT_TRUE(entity != nullptr);

    EXPECT_EQ(entity->getKeyValue("classname"), "func_static");

    EXPECT_FALSE(entity->getKeyValue("name").empty());
    EXPECT_EQ(entity->getKeyValue("model"), entity->getKeyValue("name"));
    EXPECT_EQ(entity->getKeyValue("origin"), "0 0 0");
}

TEST_F(FacadeGeneratorTest, TrimStaysInWorldspawnWhenTheEntityOptionIsOff)
{
    auto worldspawn = GlobalMapModule().findOrInsertWorldspawn();

    facade::FacadeParams params = facade::getPreset(facade::PRESET_NEWYORK);
    params.trimAsEntity = false;
    params.solidBody = false;

    auto nodes = facade::generateFacade(straightPath(800, 720), params, worldspawn);

    ASSERT_FALSE(nodes.empty());

    for (const scene::INodePtr& node : nodes)
    {
        EXPECT_EQ(node->getParent(), worldspawn);
    }
}

TEST_F(FacadeGeneratorTest, TopFloorSitsBetweenTheUpperFloorsAndTheCornice)
{
    facade::FacadeParams params = plainParams();
    params.fitToSource = false;
    params.floorCount = 2;
    params.hasTopFloor = true;
    params.top.height = 100;
    params.corniceHeight = 60;
    params.parapetHeight = 40;

    auto bands = facade::buildBands(params, 0, 0);

    ASSERT_EQ(bands.size(), 6);
    EXPECT_EQ(bands[0].kind, facade::BAND_GROUND);
    EXPECT_EQ(bands[1].kind, facade::BAND_UPPER);
    EXPECT_EQ(bands[2].kind, facade::BAND_UPPER);
    EXPECT_EQ(bands[3].kind, facade::BAND_TOP);
    EXPECT_EQ(bands[4].kind, facade::BAND_CORNICE);
    EXPECT_EQ(bands[5].kind, facade::BAND_PARAPET);

    EXPECT_NEAR(bands[3].z1 - bands[3].z0, 100, 0.01);
}

TEST_F(FacadeGeneratorTest, FitModeReservesRoomForTheTopFloor)
{
    facade::FacadeParams params = plainParams();
    params.fitToSource = true;
    params.hasTopFloor = true;
    params.top.height = 100;
    params.corniceHeight = 60;
    params.parapetHeight = 40;

    auto bands = facade::buildBands(params, 0, 720);

    ASSERT_FALSE(bands.empty());
    EXPECT_NEAR(bands.back().z1, 720, 0.01);

    int topCount = 0;

    for (const facade::Band& band : bands)
    {
        if (band.kind == facade::BAND_TOP)
        {
            ++topCount;
            EXPECT_NEAR(band.z1 - band.z0, 100, 0.01);
        }
    }

    EXPECT_EQ(topCount, 1);
}

TEST_F(FacadeGeneratorTest, TopFloorUsesItsOwnBayStyleNotTheUpperOne)
{
    auto worldspawn = GlobalMapModule().findOrInsertWorldspawn();

    facade::FacadeParams params = plainParams();
    params.fitToSource = false;
    params.floorCount = 1;
    params.hasTopFloor = true;
    params.top.height = 120;
    params.top.bayPitch = 0;

    auto boxes = boxesOf(facade::generateFacade(straightPath(), params, worldspawn));

    ASSERT_FALSE(boxes.empty());

    EXPECT_FALSE(containsPoint(boxes, Vector3(120, -10, 220)));

    EXPECT_TRUE(containsPoint(boxes, Vector3(120, -10, 340)));
}

TEST_F(FacadeGeneratorTest, ARunOfWindowsSharesOneSillBandAndOneHeadBand)
{
    facade::FacadeParams params;
    params.wallThickness = 20;
    params.minEndPier = 20;
    params.upper = ribbonStyle();

    facade::Band band{0, 120, 0, facade::BAND_UPPER};
    auto openings = facade::layoutOpenings(960, 0, params.upper, 20, false, 0, 0);

    ASSERT_EQ(openings.size(), 7);

    auto spans = facade::planWallBand(960, band, openings, params, false, false);

    EXPECT_EQ(countRole(spans, facade::SPAN_SILL_BAND), 1);
    EXPECT_EQ(countRole(spans, facade::SPAN_HEAD_BAND), 1);

    EXPECT_EQ(countRole(spans, facade::SPAN_MULLION), 6);
    EXPECT_EQ(countRole(spans, facade::SPAN_PIER), 2);

    const facade::WallSpan* sill = findRole(spans, facade::SPAN_SILL_BAND);
    ASSERT_TRUE(sill != nullptr);
    EXPECT_NEAR(sill->a0, openings.front().a0, 0.01);
    EXPECT_NEAR(sill->a1, openings.back().a1, 0.01);
}

TEST_F(FacadeGeneratorTest, MullionsAreConfinedToTheWindowHeight)
{
    facade::FacadeParams params;
    params.wallThickness = 20;
    params.minEndPier = 20;
    params.upper = ribbonStyle();

    facade::Band band{0, 120, 0, facade::BAND_UPPER};
    auto openings = facade::layoutOpenings(960, 0, params.upper, 20, false, 0, 0);
    auto spans = facade::planWallBand(960, band, openings, params, false, false);

    ASSERT_EQ(countRole(spans, facade::SPAN_MULLION), 6);

    for (const facade::WallSpan& span : spans)
    {
        if (span.role != facade::SPAN_MULLION)
        {
            continue;
        }

        EXPECT_NEAR(span.z0, openings.front().z0, 0.01);
        EXPECT_NEAR(span.z1, openings.front().z1, 0.01);
        EXPECT_GT(span.z0, band.z0 + 0.01);
        EXPECT_LT(span.z1, band.z1 - 0.01);
    }
}

TEST_F(FacadeGeneratorTest, TheDoorBreaksTheRunAndGetsNoSillBand)
{
    facade::FacadeParams params;
    params.wallThickness = 20;
    params.minEndPier = 20;
    params.ground.height = 160;
    params.ground.bayPitch = 160;
    params.ground.openingWidth = 80;
    params.ground.openingHeight = 100;
    params.ground.sillHeight = 20;

    facade::Band band{0, 160, 0, facade::BAND_GROUND};
    auto openings = facade::layoutOpenings(800, 0, params.ground, 20, true, 60, 120);

    ASSERT_EQ(openings.size(), 4);
    ASSERT_NEAR(openings[2].z0, 0, 0.01);
    ASSERT_NEAR(openings[2].a1 - openings[2].a0, 60, 0.01);

    auto spans = facade::planWallBand(800, band, openings, params, false, false);

    EXPECT_EQ(countRole(spans, facade::SPAN_HEAD_BAND), 3);
    EXPECT_EQ(countRole(spans, facade::SPAN_SILL_BAND), 2);
    EXPECT_EQ(countRole(spans, facade::SPAN_PIER), 4);

    for (const facade::WallSpan& span : spans)
    {
        if (span.role != facade::SPAN_SILL_BAND)
        {
            continue;
        }

        bool clearsDoor = span.a1 <= openings[2].a0 + 0.01 ||
                          span.a0 >= openings[2].a1 - 0.01;

        EXPECT_TRUE(clearsDoor) << "sill band [" << span.a0 << "," << span.a1
                                << "] crosses the door at [" << openings[2].a0
                                << "," << openings[2].a1 << "]";
    }
}

TEST_F(FacadeGeneratorTest, PlannedSpansExactlyTileTheWallSlabMinusTheOpenings)
{
    facade::FacadeParams params;
    params.wallThickness = 20;
    params.minEndPier = 20;
    params.upper = ribbonStyle();

    facade::Band band{0, 120, 0, facade::BAND_UPPER};
    auto openings = facade::layoutOpenings(960, 0, params.upper, 20, false, 0, 0);
    auto spans = facade::planWallBand(960, band, openings, params, false, false);

    double solid = 0;

    for (const facade::WallSpan& span : spans)
    {
        if (span.role <= facade::SPAN_MULLION)
        {
            solid += (span.a1 - span.a0) * (span.front - span.back) * (span.z1 - span.z0);
        }
    }

    EXPECT_NEAR(solid, 960.0 * 20.0 * 120.0 - 7 * (100.0 * 20.0 * 60.0), 1.0);
}

TEST_F(FacadeGeneratorTest, PilastersStandProudOfEverySolidStrip)
{
    facade::FacadeParams params;
    params.wallThickness = 20;
    params.minEndPier = 20;
    params.upper = ribbonStyle();
    params.pierProud = 20;

    facade::Band band{0, 120, 0, facade::BAND_UPPER};
    auto openings = facade::layoutOpenings(960, 0, params.upper, 20, false, 0, 0);
    auto spans = facade::planWallBand(960, band, openings, params, false, false);

    EXPECT_EQ(countRole(spans, facade::SPAN_PILASTER), 8);

    for (const facade::WallSpan& span : spans)
    {
        if (span.role != facade::SPAN_PILASTER)
        {
            continue;
        }

        EXPECT_NEAR(span.back, band.frontOffset, 0.01);
        EXPECT_NEAR(span.front, band.frontOffset + 20, 0.01);
        EXPECT_NEAR(span.z0, band.z0, 0.01);
        EXPECT_NEAR(span.z1, band.z1, 0.01);
    }
}

TEST_F(FacadeGeneratorTest, ArcadeColumnsReplacePilastersRatherThanStackingWithThem)
{
    facade::FacadeParams params = facade::getPreset(facade::PRESET_HONGKONG);
    params.pierProud = 20;

    facade::Band band{0, 160, 0, facade::BAND_GROUND};
    auto openings = facade::layoutOpenings(800, 0, params.ground, params.minEndPier,
                                           false, 0, 0);

    auto spans = facade::planWallBand(800, band, openings, params, true, false);

    EXPECT_GT(countRole(spans, facade::SPAN_COLUMN), 0);
    EXPECT_EQ(countRole(spans, facade::SPAN_PILASTER), 0);

    const facade::WallSpan* column = findRole(spans, facade::SPAN_COLUMN);
    ASSERT_TRUE(column != nullptr);
    EXPECT_NEAR(column->front, params.upper.frontOffset, 0.01);
}

TEST_F(FacadeGeneratorTest, NeighbouringSillsDoNotMeetInTheMiddleOfAThinPier)
{
    facade::FacadeParams params = facade::getPreset(facade::PRESET_NEWYORK);
    params.upper.bayCount = 5;

    facade::Band band{0, 120, 0, facade::BAND_UPPER};
    auto openings = facade::layoutOpenings(200, 0, params.upper, params.minEndPier,
                                           false, 0, 0);

    ASSERT_GE(openings.size(), 2);

    auto boxes = planBoxes(facade::planWallBand(200, band, openings, params, false, true));

    for (std::size_t i = 0; i < boxes.size(); ++i)
    {
        for (std::size_t j = i + 1; j < boxes.size(); ++j)
        {
            EXPECT_NEAR(planOverlap(boxes[i], boxes[j]), 0, 0.01);
        }
    }
}

TEST_F(FacadeGeneratorTest, ACourseRidesOverThePilastersInsteadOfCuttingThem)
{
    facade::FacadeParams params;
    params.wallThickness = 20;
    params.minEndPier = 20;
    params.upper = ribbonStyle();
    params.pierProud = 20;
    params.courseHeight = 20;
    params.courseProud = 20;

    facade::Band band{0, 120, 0, facade::BAND_UPPER};
    auto openings = facade::layoutOpenings(960, 0, params.upper, 20, false, 0, 0);
    auto spans = facade::planWallBand(960, band, openings, params, false, true);

    const facade::WallSpan* course = findRole(spans, facade::SPAN_COURSE);
    ASSERT_TRUE(course != nullptr);

    EXPECT_NEAR(course->back, band.frontOffset + 20, 0.01);
    EXPECT_NEAR(course->front, band.frontOffset + 40, 0.01);
}

TEST_F(FacadeGeneratorTest, AContinuousShopfrontIsOneWideOpeningWithEndPiers)
{
    facade::BandStyle style;
    style.bayCount = 1;
    style.bayPitch = 160;
    style.openingWidth = 400;
    style.openingHeight = 120;
    style.sillHeight = 20;

    auto openings = facade::layoutOpenings(480, 0, style, 20, false, 0, 0);

    ASSERT_EQ(openings.size(), 1);
    EXPECT_NEAR(openings[0].a0, 40, 0.01);
    EXPECT_NEAR(openings[0].a1, 440, 0.01);

    facade::FacadeParams params;
    params.wallThickness = 20;
    params.minEndPier = 20;
    params.ground.height = 160;

    facade::Band band{0, 160, 0, facade::BAND_GROUND};
    auto spans = facade::planWallBand(480, band, openings, params, false, false);

    EXPECT_EQ(countRole(spans, facade::SPAN_PIER), 2);
    EXPECT_EQ(countRole(spans, facade::SPAN_MULLION), 0);
    EXPECT_EQ(countRole(spans, facade::SPAN_SILL_BAND), 1);
    EXPECT_EQ(countRole(spans, facade::SPAN_HEAD_BAND), 1);
}

TEST_F(FacadeGeneratorTest, TheWholeFacadeNeverIntersectsItself)
{
    const int presets[] = {facade::PRESET_BLANK, facade::PRESET_HONGKONG,
                           facade::PRESET_NEWYORK, facade::PRESET_CURTAIN,
                           facade::PRESET_BRUTALIST, facade::PRESET_WAREHOUSE,
                           facade::PRESET_RIBBON};

    for (int preset : presets)
    {
        for (double length : {320.0, 480.0, 800.0, 1200.0})
        {
            for (double height : {240.0, 400.0, 600.0, 720.0})
            {
                for (int pierProud : {0, 20})
                {
                    facade::FacadeParams params = facade::getPreset(preset);
                    params.fitToSource = true;
                    params.solidBody = true;
                    params.pierProud = pierProud;

                    auto boxes = planBoxes(
                        facade::planFacade(length, 0, height, 300, params));

                    ASSERT_FALSE(boxes.empty());

                    for (std::size_t i = 0; i < boxes.size(); ++i)
                    {
                        for (std::size_t j = i + 1; j < boxes.size(); ++j)
                        {
                            EXPECT_NEAR(planOverlap(boxes[i], boxes[j]), 0, 0.01)
                                << "preset " << preset << " length " << length
                                << " height " << height << " pierProud " << pierProud
                                << " spans " << i << " and " << j;
                        }
                    }
                }
            }
        }
    }
}

TEST_F(FacadeGeneratorTest, EverySolidInTheWholeFacadeStaysOnTheGrid)
{
    const int presets[] = {facade::PRESET_BLANK, facade::PRESET_HONGKONG,
                           facade::PRESET_NEWYORK, facade::PRESET_CURTAIN,
                           facade::PRESET_BRUTALIST, facade::PRESET_WAREHOUSE,
                           facade::PRESET_RIBBON};

    for (int preset : presets)
    {
        for (double length : {320.0, 480.0, 800.0, 1200.0})
        {
            for (int bayCount : {0, 3, 6})
            {
                facade::FacadeParams params = facade::getPreset(preset);
                params.fitToSource = true;
                params.ground.bayCount = bayCount;
                params.upper.bayCount = bayCount;
                params.top.bayCount = bayCount;

                auto spans = facade::planFacade(length, 0, 720, 300, params);

                ASSERT_FALSE(spans.empty())
                    << "preset " << preset << " length " << length;

                for (const facade::WallSpan& span : spans)
                {
                    double width = span.a1 - span.a0;
                    double height = span.z1 - span.z0;
                    double depth = span.front - span.back;

                    EXPECT_NEAR(width, facade::snapTo(width, facade::GRID), 0.01)
                        << "preset " << preset << " role " << span.role;
                    EXPECT_NEAR(height, facade::snapTo(height, facade::GRID), 0.01)
                        << "preset " << preset << " role " << span.role;
                    EXPECT_NEAR(depth, facade::snapTo(depth, facade::GRID), 0.01)
                        << "preset " << preset << " role " << span.role;

                    EXPECT_GE(width, facade::GRID - 0.01);
                    EXPECT_GE(height, facade::GRID - 0.01);
                    EXPECT_GE(depth, facade::GRID - 0.01);
                }
            }
        }
    }
}

TEST_F(FacadeGeneratorTest, TheFacadeStackSpansTheWholeSourceBrush)
{
    facade::FacadeParams params = facade::getPreset(facade::PRESET_NEWYORK);
    params.fitToSource = true;

    auto spans = facade::planFacade(800, 0, 720, 0, params);

    ASSERT_FALSE(spans.empty());

    double lowest = 1e9;
    double highest = -1e9;

    for (const facade::WallSpan& span : spans)
    {
        lowest = std::min(lowest, span.z0);
        highest = std::max(highest, span.z1);
    }

    EXPECT_NEAR(lowest, 0, 0.01);
    EXPECT_NEAR(highest, 720, 0.01);
}

TEST_F(FacadeGeneratorTest, TheBodySitsEntirelyBehindTheFacadeWall)
{
    facade::FacadeParams params = facade::getPreset(facade::PRESET_NEWYORK);
    params.fitToSource = true;
    params.solidBody = true;

    auto spans = facade::planFacade(800, 0, 720, 300, params);

    const facade::WallSpan* body = findRole(spans, facade::SPAN_BODY);
    ASSERT_TRUE(body != nullptr);

    EXPECT_NEAR(body->front, -params.wallThickness, 0.01);
    EXPECT_NEAR(body->back, -params.wallThickness - 300, 0.01);

    for (const facade::WallSpan& span : spans)
    {
        if (span.role != facade::SPAN_BODY)
        {
            EXPECT_GE(span.back, body->front - 0.01)
                << "role " << span.role << " reaches into the body";
        }
    }
}

TEST_F(FacadeGeneratorTest, TheRibbonPresetProducesContinuousBandedGlazing)
{
    facade::FacadeParams params = facade::getPreset(facade::PRESET_RIBBON);
    params.fitToSource = true;

    auto spans = facade::planFacade(960, 0, 600, 0, params);

    int sillBands = 0;
    int mullions = 0;
    int pilasters = 0;

    for (const facade::WallSpan& span : spans)
    {
        if (span.role == facade::SPAN_SILL_BAND)
        {
            ++sillBands;
        }

        if (span.role == facade::SPAN_MULLION)
        {
            ++mullions;
        }

        if (span.role == facade::SPAN_PILASTER)
        {
            ++pilasters;
        }
    }

    EXPECT_GT(sillBands, 0);
    EXPECT_GT(mullions, sillBands);
    EXPECT_GT(pilasters, 0);

    for (const facade::WallSpan& span : spans)
    {
        if (span.role == facade::SPAN_MULLION)
        {
            EXPECT_GE(span.a1 - span.a0, facade::GRID - 0.01);
            EXPECT_LE(span.a1 - span.a0, 2 * facade::GRID + 0.01);
        }
    }
}

TEST_F(FacadeGeneratorTest, APathThatDoublesBackDoesNotProduceNaNGeometry)
{
    auto worldspawn = GlobalMapModule().findOrInsertWorldspawn();

    facade::FacadeParams params = plainParams();
    params.ground.height = 100;
    params.floorCount = 0;
    params.ground.bayPitch = 0;

    facade::FacadePath path;
    path.points.push_back(Vector3(0, 0, 0));
    path.points.push_back(Vector3(240, 0, 0));
    path.points.push_back(Vector3(0, 0, 0));
    path.baseZ = 0;
    path.topZ = 100;

    auto nodes = facade::generateFacade(path, params, worldspawn);

    for (const scene::INodePtr& node : nodes)
    {
        AABB bounds = node->worldAABB();

        ASSERT_TRUE(bounds.isValid());
        EXPECT_TRUE(std::isfinite(bounds.origin.x()));
        EXPECT_TRUE(std::isfinite(bounds.origin.y()));
        EXPECT_TRUE(std::isfinite(bounds.extents.x()));
        EXPECT_TRUE(std::isfinite(bounds.extents.y()));
        EXPECT_LT(bounds.extents.x(), 10000);
        EXPECT_LT(bounds.extents.y(), 10000);
    }
}

TEST_F(FacadeGeneratorTest, NothingIsGeneratedForADegeneratePath)
{
    auto worldspawn = GlobalMapModule().findOrInsertWorldspawn();

    EXPECT_TRUE(facade::generateFacade(straightPath(8), plainParams(), worldspawn).empty());
}

TEST_F(FacadeGeneratorTest, ABentPathSplitsEachRunIntoItsOwnPlanarBrush)
{
    auto worldspawn = GlobalMapModule().findOrInsertWorldspawn();

    facade::FacadeParams params = plainParams();
    params.ground.height = 100;
    params.floorCount = 0;
    params.ground.bayPitch = 0;

    auto nodes = facade::generateFacade(bentPath(), params, worldspawn);

    ASSERT_EQ(nodes.size(), 2);

    AABB combined;
    combined.includeAABB(nodes[0]->worldAABB());
    combined.includeAABB(nodes[1]->worldAABB());

    Vector3 mins = combined.origin - combined.extents;
    Vector3 maxs = combined.origin + combined.extents;

    EXPECT_NEAR(mins.x(), 0, 0.01);
    EXPECT_NEAR(mins.y(), -THICKNESS, 0.01);
    EXPECT_NEAR(maxs.x(), 240 + THICKNESS, 0.01);
    EXPECT_NEAR(maxs.y(), 240, 0.01);
    EXPECT_NEAR(maxs.z(), 100, 0.01);
}

TEST_F(FacadeGeneratorTest, TheMiteredCornerLeavesNoGapBetweenTheTwoRuns)
{
    auto worldspawn = GlobalMapModule().findOrInsertWorldspawn();

    facade::FacadeParams params = plainParams();
    params.ground.height = 100;
    params.floorCount = 0;
    params.ground.bayPitch = 0;

    auto nodes = facade::generateFacade(bentPath(), params, worldspawn);

    ASSERT_EQ(nodes.size(), 2);

    Vector3 expected = Vector3(1, 1, 0).getNormalised();
    double expectedDist = expected.dot(Vector3(240, 0, 0));

    auto findCap = [](const scene::INodePtr& node, const Vector3& normal) -> const IFace* {
        IBrush* brush = Node_getIBrush(node);

        for (std::size_t i = 0; i < brush->getNumFaces(); ++i)
        {
            if (brush->getFace(i).getPlane3().normal().dot(normal) > 0.999)
            {
                return &brush->getFace(i);
            }
        }

        return nullptr;
    };

    const IFace* firstCap = findCap(nodes[0], expected);
    const IFace* secondCap = findCap(nodes[1], -expected);

    ASSERT_TRUE(firstCap != nullptr);
    ASSERT_TRUE(secondCap != nullptr);

    EXPECT_NEAR(firstCap->getPlane3().dist(), expectedDist, 0.01);
    EXPECT_NEAR(secondCap->getPlane3().dist(), -expectedDist, 0.01);
}

TEST_F(FacadeGeneratorTest, TheSolidBodyIsKeptBehindTheFacade)
{
    auto worldspawn = GlobalMapModule().findOrInsertWorldspawn();

    facade::FacadeParams params = plainParams();
    params.solidBody = true;

    facade::FacadePath path = straightPath();
    path.bodyDepth = 300;

    auto boxes = boxesOf(facade::generateFacade(path, params, worldspawn));

    ASSERT_FALSE(boxes.empty());

    EXPECT_TRUE(containsPoint(boxes, Vector3(240, -170, 200)));

    double back = 1000;

    for (const Box& box : boxes)
    {
        back = std::min(back, box.mins.y());
    }

    EXPECT_NEAR(back, -320, 0.01);
}

TEST_F(FacadeGeneratorTest, FitTexturePutsAWholeTilePerMetreOnEveryFace)
{
    auto worldspawn = GlobalMapModule().findOrInsertWorldspawn();

    facade::FacadeParams params = plainParams();
    params.tileUnits = 40;
    params.ground.bayPitch = 0;
    params.ground.height = 120;
    params.floorCount = 0;

    auto nodes = facade::generateFacade(straightPath(480, 120), params, worldspawn);

    ASSERT_EQ(nodes.size(), 1);

    IBrush* brush = Node_getIBrush(nodes[0]);
    ASSERT_TRUE(brush != nullptr);

    bool sawFrontFace = false;

    for (std::size_t i = 0; i < brush->getNumFaces(); ++i)
    {
        IFace& face = brush->getFace(i);

        if (face.getPlane3().normal().y() < 0.99)
        {
            continue;
        }

        double minS = 1e9, maxS = -1e9, minT = 1e9, maxT = -1e9;

        for (const WindingVertex& vertex : face.getWinding())
        {
            minS = std::min(minS, vertex.texcoord.x());
            maxS = std::max(maxS, vertex.texcoord.x());
            minT = std::min(minT, vertex.texcoord.y());
            maxT = std::max(maxT, vertex.texcoord.y());
        }

        double spanS = maxS - minS;
        double spanT = maxT - minT;

        if (spanS < spanT)
        {
            std::swap(spanS, spanT);
        }

        EXPECT_NEAR(spanS, 12.0, 0.01);
        EXPECT_NEAR(spanT, 3.0, 0.01);

        sawFrontFace = true;
    }

    EXPECT_TRUE(sawFrontFace);
}

TEST_F(FacadeGeneratorTest, TheFrontSideChoicePicksTheMatchingOutwardNormal)
{
    AABB bounds = AABB::createFromMinMax(Vector3(0, 0, 0), Vector3(480, 320, 400));

    struct Expectation
    {
        int front;
        Vector3 normal;
    };

    const Expectation cases[] = {
        {facade::FRONT_YPOS, Vector3(0, 1, 0)},
        {facade::FRONT_YNEG, Vector3(0, -1, 0)},
        {facade::FRONT_XPOS, Vector3(1, 0, 0)},
        {facade::FRONT_XNEG, Vector3(-1, 0, 0)},
    };

    for (const Expectation& expected : cases)
    {
        auto path = facade::pathFromBounds(bounds, expected.front, THICKNESS);
        auto frames = facade::buildFrames(path);

        ASSERT_EQ(frames.size(), 1);
        EXPECT_NEAR(frames[0].normal.dot(expected.normal), 1.0, 0.001);
    }
}

TEST_F(FacadeGeneratorTest, TheBodyDepthExcludesTheFacadeThickness)
{
    AABB bounds = AABB::createFromMinMax(Vector3(0, 0, 0), Vector3(480, 320, 400));

    auto path = facade::pathFromBounds(bounds, facade::FRONT_YPOS, THICKNESS);

    EXPECT_NEAR(path.bodyDepth, 320 - THICKNESS, 0.01);
    EXPECT_NEAR(path.baseZ, 0, 0.01);
    EXPECT_NEAR(path.topZ, 400, 0.01);
}

} // namespace test
