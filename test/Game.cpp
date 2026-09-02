#include "RadiantTest.h"
#include "HiddenModelFilter.h"
#include "string/split.h"

namespace test
{

using GameTest = RadiantTest;

TEST_F(GameTest, GetCurrentGameConfig)
{
    // Check that we can get the game manager and current game without crashing or anything
    auto& mgr = GlobalGameManager();
    auto game = mgr.currentGame();
    ASSERT_TRUE(game);

    // RadiantTest sets up a test game type which should be exposed via the GameManager
    auto conf = mgr.getConfig();
    EXPECT_EQ(conf.gameType, RadiantTest::DEFAULT_GAME_TYPE);

    // The start of the engine path could be anywhere (depending on where the test binaries are
    // being run from), but it should end with "resources/tdm"
    auto pathComps = string::splitToVec(conf.enginePath, "/");
    EXPECT_GE(pathComps.size(), 2);
    EXPECT_EQ(pathComps.at(pathComps.size() - 1), "tdm");
    EXPECT_EQ(pathComps.at(pathComps.size() - 2), "resources");
}

TEST_F(GameTest, GetGameList)
{
    auto games = GlobalGameManager().getSortedGameList();
    ASSERT_TRUE(games.size() > 2);

    // The games are sorted by the "index" value in the .game XML file, placing TDM at the top,
    // followed by Doom 3 and its demo.
    EXPECT_EQ(games[0]->getName(), "The Dark Mod 2.0 (Standalone)");
    EXPECT_EQ(games[1]->getName(), "Doom 3");
    EXPECT_EQ(games[2]->getName(), "Doom 3 Demo");
}

TEST_F(GameTest, GetGameKeyValues)
{
    auto game = GlobalGameManager().currentGame();

    // Default game is darkmod.game
    EXPECT_EQ(game->getKeyValue("type"), "doom3");
    EXPECT_EQ(game->getKeyValue("name"), "The Dark Mod 2.0 (Standalone)");
    EXPECT_EQ(game->getKeyValue("index"), "10");
    EXPECT_EQ(game->getKeyValue("maptypes"), "mapdoom3");
}

TEST_F(GameTest, GuiPropertyTypeRegistered)
{
    // adding the GUI chooser requires registering type="gui" for keys named
    // "gui" and "gui<digits>" so the editor is picked up
    auto game = GlobalGameManager().currentGame();

    auto guiNodes = game->getLocalXPath("/entityInspector//property[@match='gui']");
    ASSERT_EQ(guiNodes.size(), 1u) << "darkmod.game should declare a property mapping for the 'gui' key";
    EXPECT_EQ(guiNodes[0].getAttributeValue("type"), "gui");

    auto numberedGuiNodes = game->getLocalXPath("/entityInspector//property[@match='gui[0-9]+']");
    ASSERT_EQ(numberedGuiNodes.size(), 1u) << "darkmod.game should declare a property mapping for 'gui<n>' keys";
    EXPECT_EQ(numberedGuiNodes[0].getAttributeValue("type"), "gui");
}

TEST_F(GameTest, GetOptionalFeatures)
{
    auto games = GlobalGameManager().getSortedGameList();

    auto tdm = std::find_if(games.begin(), games.end(), [](game::IGamePtr g) {
        return g->getName() == "The Dark Mod 2.0 (Standalone)";
    });
    ASSERT_TRUE(tdm != games.end());
    auto q3 = std::find_if(games.begin(), games.end(), [](game::IGamePtr g) {
        return g->getName() == "Quake 3";
    });
    ASSERT_TRUE(q3 != games.end());

    // Only Quake 3 should have the "detail_brushes" feature
    EXPECT_FALSE((*tdm)->hasFeature("detail_brushes"));
    EXPECT_TRUE((*q3)->hasFeature("detail_brushes"));

    // Only Dark Mod should have the "hot_reload" feature
    EXPECT_TRUE((*tdm)->hasFeature("hot_reload"));
    EXPECT_FALSE((*q3)->hasFeature("hot_reload"));
}

TEST_F(GameTest, HiddenModelPatterns)
{
    auto games = GlobalGameManager().getSortedGameList();

    auto hellcore = std::find_if(games.begin(), games.end(), [](game::IGamePtr g) {
        return g->getName() == "HellCore";
    });
    ASSERT_TRUE(hellcore != games.end());

    auto nodes = (*hellcore)->getLocalXPath(game::HiddenModelFilter::XPATH);
    ASSERT_EQ(nodes.size(), 2u) << "hellcore.game should hide the _coll and _lod siblings";

    game::HiddenModelFilter filter(nodes);

    EXPECT_TRUE(filter.isHidden("asylum/armchair_coll.ase"));
    EXPECT_TRUE(filter.isHidden("asylum/armchair_lod1.ase"));
    EXPECT_TRUE(filter.isHidden("props/christmas_gifts_02_lod12.ase"));

    EXPECT_FALSE(filter.isHidden("asylum/armchair.ase"));
    EXPECT_FALSE(filter.isHidden("policedept/paper_clip.ase"));
    EXPECT_FALSE(filter.isHidden("topdown/fence_collumn_01.ase"));
    EXPECT_FALSE(filter.isHidden("props/human_collar_01.ase"));

    // A game that declares no patterns hides nothing
    auto q3 = std::find_if(games.begin(), games.end(), [](game::IGamePtr g) {
        return g->getName() == "Quake 3";
    });
    ASSERT_TRUE(q3 != games.end());

    game::HiddenModelFilter noPatterns((*q3)->getLocalXPath(game::HiddenModelFilter::XPATH));
    EXPECT_FALSE(noPatterns.isHidden("asylum/armchair_coll.ase"));
}

}
