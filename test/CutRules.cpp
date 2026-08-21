#include "gtest/gtest.h"
#include "ui/cut/CutRules.h"

namespace test
{

namespace
{

const double POSITION_TOLERANCE = 0.000001;

void expectPositions(const std::vector<double>& actual, const std::vector<double>& expected)
{
    ASSERT_EQ(actual.size(), expected.size());

    for (std::size_t i = 0; i < expected.size(); ++i)
    {
        EXPECT_NEAR(actual[i], expected[i], POSITION_TOLERANCE);
    }
}

} // anonymous namespace

TEST(CutRulesTest, EqualPartsInHalf)
{
    cut::CutRule rule;
    rule.type = cut::RULE_EQUAL_PARTS;
    rule.parts = 2;

    expectPositions(cut::computeCutPositions(rule, 0, 64), {32});
}

TEST(CutRulesTest, EqualPartsInThirds)
{
    cut::CutRule rule;
    rule.type = cut::RULE_EQUAL_PARTS;
    rule.parts = 3;

    expectPositions(cut::computeCutPositions(rule, 0, 90), {30, 60});
}

TEST(CutRulesTest, EqualPartsRejectsSinglePart)
{
    cut::CutRule rule;
    rule.type = cut::RULE_EQUAL_PARTS;
    rule.parts = 1;

    EXPECT_TRUE(cut::computeCutPositions(rule, 0, 64).empty());
}

TEST(CutRulesTest, SpacingSuppressesBrushEdges)
{
    cut::CutRule rule;
    rule.type = cut::RULE_SPACING;
    rule.step = 32;

    expectPositions(cut::computeCutPositions(rule, 0, 64), {32});
}

TEST(CutRulesTest, SpacingWithOneCutBetween)
{
    cut::CutRule rule;
    rule.type = cut::RULE_SPACING;
    rule.step = 10;
    rule.subdivisions = 1;

    expectPositions(cut::computeCutPositions(rule, 0, 30), {5, 10, 15, 20, 25});
}

TEST(CutRulesTest, SpacingHonoursOffset)
{
    cut::CutRule rule;
    rule.type = cut::RULE_SPACING;
    rule.step = 50;
    rule.offset = 10;

    expectPositions(cut::computeCutPositions(rule, 0, 100), {10, 60});
}

TEST(CutRulesTest, SpacingFromWorldOriginAlignsOffGridBrush)
{
    cut::CutRule rule;
    rule.type = cut::RULE_SPACING;
    rule.step = 20;
    rule.anchor = cut::ANCHOR_WORLD_ORIGIN;

    expectPositions(cut::computeCutPositions(rule, 47, 107), {60, 80, 100});
}

TEST(CutRulesTest, SpacingFromBrushStartIgnoresWorldGrid)
{
    cut::CutRule rule;
    rule.type = cut::RULE_SPACING;
    rule.step = 20;
    rule.anchor = cut::ANCHOR_BRUSH_MIN;

    expectPositions(cut::computeCutPositions(rule, 47, 107), {67, 87});
}

TEST(CutRulesTest, SpacingRejectsZeroStep)
{
    cut::CutRule rule;
    rule.type = cut::RULE_SPACING;
    rule.step = 0;

    EXPECT_TRUE(cut::computeCutPositions(rule, 0, 64).empty());
}

TEST(CutRulesTest, SpacingRejectsCutCountAboveLimit)
{
    cut::CutRule rule;
    rule.type = cut::RULE_SPACING;
    rule.step = 1;

    EXPECT_TRUE(cut::computeCutPositions(rule, 0, 1000).empty());
}

TEST(CutRulesTest, PatternRepeatsSegmentLengths)
{
    cut::CutRule rule;
    rule.type = cut::RULE_PATTERN;
    rule.pattern = {10, 5};

    expectPositions(cut::computeCutPositions(rule, 0, 45), {10, 15, 25, 30, 40});
}

TEST(CutRulesTest, PatternFromWorldOriginWalksBackToBrush)
{
    cut::CutRule rule;
    rule.type = cut::RULE_PATTERN;
    rule.pattern = {10, 5};
    rule.anchor = cut::ANCHOR_WORLD_ORIGIN;

    expectPositions(cut::computeCutPositions(rule, 32, 64), {40, 45, 55, 60});
}

TEST(CutRulesTest, PatternRejectsZeroLengthSegment)
{
    cut::CutRule rule;
    rule.type = cut::RULE_PATTERN;
    rule.pattern = {10, 0};

    EXPECT_TRUE(cut::computeCutPositions(rule, 0, 45).empty());
}

TEST(CutRulesTest, PatternRejectsEmptyList)
{
    cut::CutRule rule;
    rule.type = cut::RULE_PATTERN;

    EXPECT_TRUE(cut::computeCutPositions(rule, 0, 45).empty());
}

TEST(CutRulesTest, DegenerateExtentYieldsNoCuts)
{
    cut::CutRule rule;
    rule.type = cut::RULE_EQUAL_PARTS;
    rule.parts = 4;

    EXPECT_TRUE(cut::computeCutPositions(rule, 24, 24).empty());
}

} // namespace test
