#include "RadiantTest.h"

#include "ibrush.h"
#include "icommandsystem.h"
#include "iimage.h"
#include "imap.h"
#include "ipatch.h"
#include "iselection.h"
#include "scenelib.h"
#include "registry/registry.h"

#include "hotspot/HotspotDetect.h"
#include "hotspot/HotspotFit.h"
#include "hotspot/HotspotLookup.h"
#include "hotspot/HotspotSheet.h"

#include "algorithm/Primitives.h"
#include "testutil/CommandFailureHelper.h"

#include <cmath>
#include <optional>
#include <set>

namespace test
{

using HotspotTest = RadiantTest;

namespace
{

const char* const HOTSPOT_MATERIAL = "textures/hotspot/concrete_hotspot_01";

hotspot::Sheet createSheet()
{
    hotspot::Sheet sheet;
    sheet.width = 512;
    sheet.height = 512;

    sheet.rects.push_back({ 0, 0, 256, 256 });
    sheet.rects.push_back({ 256, 0, 512, 128 });
    sheet.rects.push_back({ 0, 496, 512, 512 });

    return sheet;
}

void getTexcoordBounds(const IFace& face, Vector2& mins, Vector2& maxs)
{
    mins = maxs = face.getWinding().front().texcoord;

    for (const auto& vertex : face.getWinding())
    {
        mins.x() = std::min(mins.x(), vertex.texcoord.x());
        mins.y() = std::min(mins.y(), vertex.texcoord.y());
        maxs.x() = std::max(maxs.x(), vertex.texcoord.x());
        maxs.y() = std::max(maxs.y(), vertex.texcoord.y());
    }
}

IFace* findFace(const scene::INodePtr& brushNode, const Vector3& normal)
{
    auto* brush = Node_getIBrush(brushNode);

    for (std::size_t i = 0; i < brush->getNumFaces(); ++i)
    {
        if (math::isNear(brush->getFace(i).getPlane3().normal(), normal, 0.001))
        {
            return &brush->getFace(i);
        }
    }

    return nullptr;
}

Vector2 getTexcoordAt(const IFace& face, const Vector3& point)
{
    for (const auto& vertex : face.getWinding())
    {
        if (math::isNear(vertex.vertex, point, 0.01))
        {
            return vertex.texcoord;
        }
    }

    return Vector2(-1000, -1000);
}

std::optional<Vector3> findSharedVertex(const IFace& a, const IFace& b)
{
    for (const auto& va : a.getWinding())
    {
        for (const auto& vb : b.getWinding())
        {
            if (math::isNear(va.vertex, vb.vertex, 0.01))
            {
                return va.vertex;
            }
        }
    }

    return std::nullopt;
}

bool isNearAny(int value, const std::set<int>& lines, int tolerance)
{
    for (auto line : lines)
    {
        if (std::abs(value - line) <= tolerance)
        {
            return true;
        }
    }

    return false;
}

void expectRectsOnGrid(const std::vector<hotspot::Rect>& rects, int width, int height,
    const std::set<int>& columns, const std::set<int>& rows, int tolerance)
{
    long long area = 0;

    for (const auto& rect : rects)
    {
        EXPECT_TRUE(isNearAny(rect.minX, columns, tolerance)) << "Unexpected column " << rect.minX;
        EXPECT_TRUE(isNearAny(rect.maxX, columns, tolerance)) << "Unexpected column " << rect.maxX;
        EXPECT_TRUE(isNearAny(rect.minY, rows, tolerance)) << "Unexpected row " << rect.minY;
        EXPECT_TRUE(isNearAny(rect.maxY, rows, tolerance)) << "Unexpected row " << rect.maxY;

        area += static_cast<long long>(rect.width()) * rect.height();
    }

    EXPECT_EQ(area, static_cast<long long>(width) * height) << "Detected rects must cover the image exactly";
}

std::vector<hotspot::Rect> detectFromImage(const std::string& filePath, int& width, int& height)
{
    auto image = GlobalImageLoader().imageFromFile(filePath);

    if (!image)
    {
        return {};
    }

    width = static_cast<int>(image->getWidth());
    height = static_cast<int>(image->getHeight());

    return hotspot::detectRects(image->getPixels(), width, height);
}

}

TEST(HotspotSheet, ParseHammerText)
{
    auto sheet = hotspot::parseText(R"(
// exported by RectMaker
Rectangles
{
    rectangle
    {
        min "0 0"
        max "256 128"
    }
    Rectangle
    {
        MIN "256 0"
        max "512 512"
        reflect 1
        alt 1
    }
    rectangle
    {
        min "10 10"
        max "10 20"
    }
}
)");

    ASSERT_EQ(sheet.rects.size(), 2) << "The degenerate rect must be dropped";
    EXPECT_EQ(sheet.width, 0);
    EXPECT_EQ(sheet.rects[0].maxX, 256);
    EXPECT_EQ(sheet.rects[0].maxY, 128);
    EXPECT_FALSE(sheet.rects[0].reflect);
    EXPECT_EQ(sheet.rects[1].minX, 256);
    EXPECT_TRUE(sheet.rects[1].reflect);
    EXPECT_TRUE(sheet.rects[1].alt);
    EXPECT_FALSE(sheet.rects[1].rotate);
}

TEST(HotspotSheet, ParseBinary)
{
    std::string data = { 1, 0, 2, 0 };

    auto appendRect = [&](unsigned char flags, int x0, int y0, int x1, int y1)
    {
        data += static_cast<char>(flags);

        for (auto value : { x0, y0, x1, y1 })
        {
            data += static_cast<char>(value & 0xff);
            data += static_cast<char>((value >> 8) & 0xff);
        }
    };

    appendRect(0x1 | 0x4, 0, 0, 1024, 512);
    appendRect(0x2, 512, 512, 1024, 1024);

    auto sheet = hotspot::parse(data);

    ASSERT_EQ(sheet.rects.size(), 2);
    EXPECT_TRUE(sheet.rects[0].rotate);
    EXPECT_FALSE(sheet.rects[0].reflect);
    EXPECT_TRUE(sheet.rects[0].alt);
    EXPECT_EQ(sheet.rects[0].maxX, 1024);
    EXPECT_EQ(sheet.rects[0].maxY, 512);
    EXPECT_TRUE(sheet.rects[1].reflect);
    EXPECT_EQ(sheet.rects[1].minX, 512);
}

TEST(HotspotSheet, WriteRoundTrip)
{
    auto sheet = createSheet();
    sheet.scale = 0.25;
    sheet.inset = 2;
    sheet.rects[1].rotate = true;
    sheet.rects[1].reflect = true;
    sheet.rects[2].alt = true;

    auto parsed = hotspot::parse(hotspot::writeText(sheet));

    EXPECT_EQ(parsed.width, 512);
    EXPECT_EQ(parsed.height, 512);
    EXPECT_DOUBLE_EQ(parsed.scale, 0.25);
    EXPECT_DOUBLE_EQ(parsed.inset, 2);
    ASSERT_EQ(parsed.rects.size(), sheet.rects.size());

    for (std::size_t i = 0; i < sheet.rects.size(); ++i)
    {
        EXPECT_EQ(parsed.rects[i].minX, sheet.rects[i].minX);
        EXPECT_EQ(parsed.rects[i].minY, sheet.rects[i].minY);
        EXPECT_EQ(parsed.rects[i].maxX, sheet.rects[i].maxX);
        EXPECT_EQ(parsed.rects[i].maxY, sheet.rects[i].maxY);
        EXPECT_EQ(parsed.rects[i].rotate, sheet.rects[i].rotate);
        EXPECT_EQ(parsed.rects[i].reflect, sheet.rects[i].reflect);
        EXPECT_EQ(parsed.rects[i].alt, sheet.rects[i].alt);
    }
}

TEST(HotspotFit, PicksMatchingAspectAndSize)
{
    auto sheet = createSheet();

    auto wide = hotspot::findBestPlacements(sheet, 128, 64, 0.5, false);
    ASSERT_EQ(wide.size(), 1);
    EXPECT_EQ(wide[0].rect, 1);
    EXPECT_TRUE(math::isNear(wide[0].topLeft, Vector2(0.5, 0), 1e-6));
    EXPECT_TRUE(math::isNear(wide[0].topRight, Vector2(1, 0), 1e-6));
    EXPECT_TRUE(math::isNear(wide[0].bottomLeft, Vector2(0.5, 0.25), 1e-6));

    auto square = hotspot::findBestPlacements(sheet, 128, 128, 0.5, false);
    ASSERT_EQ(square.size(), 1);
    EXPECT_EQ(square[0].rect, 0);
}

TEST(HotspotFit, RotatesOnlyWhenAllowed)
{
    auto sheet = createSheet();
    sheet.rects.erase(sheet.rects.begin());

    auto unrotated = hotspot::findBestPlacements(sheet, 64, 128, 0.5, false);
    ASSERT_FALSE(unrotated.empty());
    EXPECT_TRUE(math::isNear(unrotated[0].topRight - unrotated[0].topLeft, Vector2(0.5, 0), 1e-6));

    sheet.rects[0].rotate = true;

    auto rotated = hotspot::findBestPlacements(sheet, 64, 128, 0.5, false);
    ASSERT_EQ(rotated.size(), 1);
    EXPECT_EQ(rotated[0].rect, 0);
    EXPECT_TRUE(math::isNear(rotated[0].topRight - rotated[0].topLeft, Vector2(0, -0.25), 1e-6))
        << "Moving right along a tall face must run along the rect's V axis";
    EXPECT_TRUE(math::isNear(rotated[0].bottomLeft - rotated[0].topLeft, Vector2(0.5, 0), 1e-6));
}

TEST(HotspotFit, AllowRotationOverridesMissingFlag)
{
    auto sheet = createSheet();
    sheet.rects.erase(sheet.rects.begin());

    auto rotated = hotspot::findBestPlacements(sheet, 64, 128, 0.5, false, true);
    ASSERT_EQ(rotated.size(), 1);
    EXPECT_EQ(rotated[0].rect, 0);
    EXPECT_TRUE(math::isNear(rotated[0].topRight - rotated[0].topLeft, Vector2(0, -0.25), 1e-6))
        << "With rotation allowed globally a wide rect must turn to fit a tall face";
}

TEST(HotspotFit, SquareRectsStayUprightWhenRotationIsAllowed)
{
    auto sheet = createSheet();

    auto placements = hotspot::findBestPlacements(sheet, 128, 128, 0.5, false, true);
    ASSERT_EQ(placements.size(), 1);
    EXPECT_TRUE(math::isNear(placements[0].topRight - placements[0].topLeft, Vector2(0.5, 0), 1e-6))
        << "Rotation must only win when it improves the fit";
}

TEST(HotspotFit, FullWidthStripTiles)
{
    auto sheet = createSheet();

    auto placements = hotspot::findBestPlacements(sheet, 1024, 8, 0.5, false);
    ASSERT_EQ(placements.size(), 1);
    EXPECT_EQ(placements[0].rect, 2);
    EXPECT_NEAR(placements[0].topRight.x() - placements[0].topLeft.x(), 4.0, 1e-6)
        << "A full width strip must repeat along U instead of stretching";
    EXPECT_NEAR(placements[0].bottomLeft.y() - placements[0].topLeft.y(), 16.0 / 512, 1e-6);
}

TEST(HotspotFit, InsetShrinksNonTilingAxes)
{
    auto sheet = createSheet();
    sheet.inset = 4;

    auto square = hotspot::findBestPlacements(sheet, 128, 128, 0.5, false);
    ASSERT_EQ(square.size(), 1);
    EXPECT_TRUE(math::isNear(square[0].topLeft, Vector2(4.0 / 512, 4.0 / 512), 1e-9));
    EXPECT_TRUE(math::isNear(square[0].topRight, Vector2(252.0 / 512, 4.0 / 512), 1e-9));

    auto strip = hotspot::findBestPlacements(sheet, 1024, 8, 0.5, false);
    ASSERT_EQ(strip.size(), 1);
    EXPECT_NEAR(strip[0].topLeft.x(), 0, 1e-9) << "The tiling axis must not be inset";
    EXPECT_NEAR(strip[0].topLeft.y(), 500.0 / 512, 1e-9);
}

TEST(HotspotFit, AltSelectsAlternateRects)
{
    auto sheet = createSheet();
    sheet.rects.push_back({ 256, 256, 512, 512 });
    sheet.rects.back().alt = true;

    auto normal = hotspot::findBestPlacements(sheet, 128, 128, 0.5, false);
    ASSERT_EQ(normal.size(), 1);
    EXPECT_EQ(normal[0].rect, 0);

    auto alternate = hotspot::findBestPlacements(sheet, 128, 128, 0.5, true);
    ASSERT_EQ(alternate.size(), 1);
    EXPECT_EQ(alternate[0].rect, 3);
}

TEST(HotspotFit, RepeatedFitCyclesToAnotherRect)
{
    auto sheet = createSheet();
    sheet.rects.push_back({ 256, 256, 512, 512 });

    std::mt19937 rng(7);

    for (int i = 0; i < 10; ++i)
    {
        auto first = hotspot::choosePlacement(sheet, 128, 128, 0.5, false, std::nullopt, rng);
        ASSERT_TRUE(first);

        auto second = hotspot::choosePlacement(sheet, 128, 128, 0.5, false, first, rng);
        ASSERT_TRUE(second);
        EXPECT_NE(first->rect, second->rect);
    }
}

TEST(HotspotFit, ReflectFlipsAcrossTheRect)
{
    auto sheet = createSheet();
    sheet.rects[0].reflect = true;

    std::mt19937 rng(3);
    std::set<double> leftEdges;

    for (int i = 0; i < 32; ++i)
    {
        auto placement = hotspot::choosePlacement(sheet, 128, 128, 0.5, false, std::nullopt, rng);
        ASSERT_TRUE(placement);
        leftEdges.insert(placement->topLeft.x());

        Vector2 mins, maxs;
        hotspot::getBounds(*placement, mins, maxs);
        EXPECT_TRUE(math::isNear(mins, Vector2(0, 0), 1e-9));
        EXPECT_TRUE(math::isNear(maxs, Vector2(0.5, 0.5), 1e-9));
    }

    EXPECT_EQ(leftEdges, std::set<double>({ 0.0, 0.5 })) << "Both orientations should occur";
}

TEST(HotspotDetect, SnapRectsToGrid)
{
    std::vector<hotspot::Rect> rects = { { 0, 0, 255, 15 }, { 255, 0, 512, 15 }, { 9, 15, 11, 31 } };

    auto snapped = hotspot::snapRects(rects, 8, 512, 512);

    ASSERT_EQ(snapped.size(), 2) << "Rects collapsing to zero size must be dropped";
    EXPECT_EQ(snapped[0].maxX, 256);
    EXPECT_EQ(snapped[0].maxY, 16);
    EXPECT_EQ(snapped[1].minX, 256);
    EXPECT_EQ(snapped[1].maxX, 512);
}

TEST_F(HotspotTest, DetectConcreteGrid)
{
    int width = 0, height = 0;
    auto rects = detectFromImage(_context.getTestProjectPath() + "textures/hotspot/concrete_hotspot_01.png", width, height);

    ASSERT_EQ(width, 512);
    EXPECT_GE(rects.size(), 30);
    EXPECT_LE(rects.size(), 45);

    expectRectsOnGrid(rects, width, height,
        { 0, 256, 384, 448, 480, 496, 512 }, { 0, 16, 32, 64, 128, 256, 512 }, 2);
}

TEST_F(HotspotTest, FindSheetByMaterialName)
{
    auto loaded = hotspot::findSheet(HOTSPOT_MATERIAL);

    ASSERT_TRUE(loaded);
    EXPECT_EQ(loaded->path, std::string(HOTSPOT_MATERIAL) + ".rect");
    EXPECT_EQ(loaded->sheet.width, 512);
    EXPECT_DOUBLE_EQ(loaded->sheet.scale, 0.5);
    EXPECT_EQ(loaded->sheet.rects.size(), 5);

    EXPECT_FALSE(hotspot::findSheet("textures/numbers/1"));
}

TEST_F(HotspotTest, MapOverrideTakesPrecedence)
{
    GlobalMapModule().findOrInsertWorldspawn();
    auto root = GlobalMapModule().getRoot();
    ASSERT_TRUE(root);

    root->setProperty(std::string(hotspot::MAP_PROPERTY_PREFIX) + HOTSPOT_MATERIAL, "textures/hotspot/override.rect");

    auto loaded = hotspot::findSheet(HOTSPOT_MATERIAL);
    ASSERT_TRUE(loaded);
    EXPECT_EQ(loaded->path, "textures/hotspot/override.rect");
    EXPECT_EQ(loaded->sheet.rects.size(), 1);

    root->removeProperty(std::string(hotspot::MAP_PROPERTY_PREFIX) + HOTSPOT_MATERIAL);

    loaded = hotspot::findSheet(HOTSPOT_MATERIAL);
    ASSERT_TRUE(loaded);
    EXPECT_EQ(loaded->sheet.rects.size(), 5);
}

TEST_F(HotspotTest, ApplyingHotspotMaterialFitsEveryFace)
{
    auto worldspawn = GlobalMapModule().findOrInsertWorldspawn();
    auto brush = algorithm::createCubicBrush(worldspawn, Vector3(0, 0, 0), "_default");
    Node_setSelected(brush, true);

    GlobalCommandSystem().executeCommand("SetShaderOnSelection", std::string(HOTSPOT_MATERIAL));

    auto* ibrush = Node_getIBrush(brush);

    for (std::size_t i = 0; i < ibrush->getNumFaces(); ++i)
    {
        const auto& face = ibrush->getFace(i);
        EXPECT_EQ(face.getShader(), HOTSPOT_MATERIAL);

        Vector2 mins, maxs;
        getTexcoordBounds(face, mins, maxs);

        EXPECT_NEAR(maxs.x() - mins.x(), 0.5, 1e-4) << "Face " << i << " should span a 256 pixel rect";
        EXPECT_NEAR(maxs.y() - mins.y(), 0.5, 1e-4) << "Face " << i << " should span a 256 pixel rect";

        bool firstRect = math::isNear(mins, Vector2(0, 0), 1e-4);
        bool secondRect = math::isNear(mins, Vector2(0.5, 0.5), 1e-4);
        EXPECT_TRUE(firstRect || secondRect) << "Face " << i << " is not aligned to a square rect: " << mins;
    }
}

TEST_F(HotspotTest, WallsKeepTheRectUpright)
{
    auto worldspawn = GlobalMapModule().findOrInsertWorldspawn();
    auto brush = algorithm::createCubicBrush(worldspawn, Vector3(0, 0, 0), HOTSPOT_MATERIAL);
    Node_setSelected(brush, true);

    GlobalCommandSystem().executeCommand("HotspotTexture");

    auto* wall = findFace(brush, Vector3(1, 0, 0));
    ASSERT_TRUE(wall);

    auto top = getTexcoordAt(*wall, Vector3(64, -64, 64));
    auto bottom = getTexcoordAt(*wall, Vector3(64, -64, -64));

    EXPECT_LT(top.y(), bottom.y()) << "The top of a wall must map to the top of the rect";
}

TEST_F(HotspotTest, HotspotCommandCyclesRects)
{
    auto worldspawn = GlobalMapModule().findOrInsertWorldspawn();
    auto brush = algorithm::createCubicBrush(worldspawn, Vector3(0, 0, 0), HOTSPOT_MATERIAL);
    Node_setSelected(brush, true);

    auto* face = findFace(brush, Vector3(0, 0, 1));
    ASSERT_TRUE(face);

    GlobalCommandSystem().executeCommand("HotspotTexture");

    Vector2 firstMins, firstMaxs;
    getTexcoordBounds(*face, firstMins, firstMaxs);

    GlobalCommandSystem().executeCommand("HotspotTexture");

    Vector2 secondMins, secondMaxs;
    getTexcoordBounds(*face, secondMins, secondMaxs);

    EXPECT_FALSE(math::isNear(firstMins, secondMins, 1e-4)) << "Repeating the command must pick the other square rect";
}

TEST_F(HotspotTest, MergedCoplanarFacesShareOneMapping)
{
    auto worldspawn = GlobalMapModule().findOrInsertWorldspawn();
    auto left = algorithm::createCubicBrush(worldspawn, Vector3(0, 0, 0), HOTSPOT_MATERIAL);
    auto right = algorithm::createCubicBrush(worldspawn, Vector3(128, 0, 0), HOTSPOT_MATERIAL);

    auto* leftTop = findFace(left, Vector3(0, 0, 1));
    auto* rightTop = findFace(right, Vector3(0, 0, 1));
    ASSERT_TRUE(leftTop && rightTop);

    auto shared = findSharedVertex(*leftTop, *rightTop);
    ASSERT_TRUE(shared) << "The two top faces must touch";

    Node_setSelected(left, true);
    Node_setSelected(right, true);

    registry::setValue(hotspot::RKEY_MERGE_COPLANAR, true);
    GlobalCommandSystem().executeCommand("HotspotTexture");
    registry::setValue(hotspot::RKEY_MERGE_COPLANAR, false);

    EXPECT_TRUE(math::isNear(getTexcoordAt(*leftTop, *shared), getTexcoordAt(*rightTop, *shared), 1e-4))
        << "Merged faces must continue the same mapping across the shared edge";

    auto* leftSide = findFace(left, Vector3(0, -1, 0));
    auto* rightSide = findFace(right, Vector3(0, -1, 0));
    ASSERT_TRUE(leftSide && rightSide);

    auto sharedSide = findSharedVertex(*leftSide, *rightSide);
    ASSERT_TRUE(sharedSide);
    EXPECT_TRUE(math::isNear(getTexcoordAt(*leftSide, *sharedSide), getTexcoordAt(*rightSide, *sharedSide), 1e-4));
}

TEST_F(HotspotTest, PatchIsFittedToARect)
{
    auto worldspawn = GlobalMapModule().findOrInsertWorldspawn();
    auto patchNode = algorithm::createPatchFromBounds(worldspawn,
        AABB(Vector3(32, 32, 0), Vector3(32, 32, 0)), HOTSPOT_MATERIAL);
    Node_setSelected(patchNode, true);

    GlobalCommandSystem().executeCommand("HotspotTexture");

    auto* patch = Node_getIPatch(patchNode);

    Vector2 mins = patch->ctrlAt(0, 0).texcoord;
    Vector2 maxs = mins;

    for (std::size_t row = 0; row < patch->getHeight(); ++row)
    {
        for (std::size_t col = 0; col < patch->getWidth(); ++col)
        {
            const auto& texcoord = patch->ctrlAt(row, col).texcoord;
            mins.x() = std::min(mins.x(), texcoord.x());
            mins.y() = std::min(mins.y(), texcoord.y());
            maxs.x() = std::max(maxs.x(), texcoord.x());
            maxs.y() = std::max(maxs.y(), texcoord.y());
        }
    }

    EXPECT_NEAR(maxs.x() - mins.x(), 0.5, 1e-4);
    EXPECT_NEAR(maxs.y() - mins.y(), 0.5, 1e-4);
    EXPECT_TRUE(math::isNear(mins, Vector2(0, 0), 1e-4) || math::isNear(mins, Vector2(0.5, 0.5), 1e-4));
}

TEST_F(HotspotTest, MaterialWithoutRectsKeepsItsProjection)
{
    auto worldspawn = GlobalMapModule().findOrInsertWorldspawn();
    auto brush = algorithm::createCubicBrush(worldspawn, Vector3(0, 0, 0), "_default");
    Node_setSelected(brush, true);

    auto* face = findFace(brush, Vector3(0, 0, 1));
    ASSERT_TRUE(face);

    GlobalCommandSystem().executeCommand("SetShaderOnSelection", std::string("textures/numbers/1"));
    auto before = face->getWinding();

    CommandFailureHelper failureHelper;
    GlobalCommandSystem().executeCommand("HotspotTexture");

    EXPECT_TRUE(failureHelper.executionHasNotBeenPossible());

    for (std::size_t i = 0; i < before.size(); ++i)
    {
        EXPECT_TRUE(math::isNear(face->getWinding()[i].texcoord, before[i].texcoord, 1e-9));
    }
}

}
