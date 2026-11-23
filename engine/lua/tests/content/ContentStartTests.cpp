#include <gtest/gtest.h>

#include <string>

#include "haylen/content/ContentManager.hpp"
#include "haylen/core/AppError.hpp"
#include "haylen/core/Engine.hpp"
#include "haylen/io/MemoryPackage.hpp"
#include "support/ContentAppFixture.hpp"
#include "support/TestFiles.hpp"

namespace haylen::content {

// An app whose `haylen.json` names content that the engine reads before the app starts: a folder of languages and the group manifest of its delivery rules, which a release may not hold on the device yet, as on a web page.
class ContentStartTest : public test::ContentAppFixture {
  protected:
    // Builds and publishes the game again with the languages and the sections of `haylen.json` that read them.
    void rebuild(const std::string& sections) {
        game["haylen.json"] = test::TestFiles::bytes(R"({"name": "Start", "identifier": "com.haylen.tests", )" + sections + "}");
        game["source/main.lua"] = test::TestFiles::bytes("greeting = require('haylen.localization').text('menu.play')\nstarted = true");
        game.emplace("content/locale/en.json", test::TestFiles::bytes(R"({"menu": {"play": "Play"}})"));
        game.emplace("content/locale/pt-BR.json", test::TestFiles::bytes(R"({"menu": {"play": "Jogar"}})"));
        game.emplace("content/groups.json", test::TestFiles::bytes(R"({"groups": {"forest": ["levels/"]}})"));
        release.build(io::MemoryPackage("source", game));
        publish(game);
    }

    [[nodiscard]] std::string getServer() const {
        return R"("updates": {"baseUrl": ")" + server.getUrl() + R"(", "enabled": false})";
    }
};

TEST_F(ContentStartTest, LoadsTheLanguagesOfAppJsonOnceTheyDownload) {
    rebuild(R"("localization": {"folder": "locale", "fallback": "en"}, "content": {)" + getServer() + "}");
    run(shipWithoutContent());

    EXPECT_FALSE(hasStarted());
    ASSERT_TRUE(frameUntil([this] { return hasStarted(); })) << (engine->getError() != nullptr ? engine->getError()->getMessage() : "");
    EXPECT_EQ(getGlobalText("greeting"), "Play") << "The main module reads the languages, so they loaded before it ran.";
    EXPECT_TRUE(engine->getContent().isLocal("locale/pt-BR.json"));
}

TEST_F(ContentStartTest, BringsTheLanguagesThatARuleDeliversLaterBeforeTheAppStarts) {
    rebuild(R"("localization": {"folder": "locale", "fallback": "en"}, "content": {)" + getServer() + R"(, "delivery": [{"match": "locale/**", "mode": "onDemand"}]})");
    run(ship());

    EXPECT_FALSE(engine->getContent().isLocal("locale/en.json"));
    ASSERT_TRUE(frameUntil([this] { return hasStarted(); })) << (engine->getError() != nullptr ? engine->getError()->getMessage() : "");
    EXPECT_EQ(getGlobalText("greeting"), "Play");
    EXPECT_TRUE(engine->getContent().isLocal("locale/en.json"));
}

TEST_F(ContentStartTest, ShowsWhyTheLanguagesCannotDownloadAndTriesAgain) {
    rebuild(R"("localization": {"folder": "locale", "fallback": "en"}, "content": {)" + getServer() + R"(, "delivery": [{"match": "locale/**", "mode": "onDemand"}]})");
    setPacksStatus(503);
    run(ship());

    ASSERT_TRUE(frameUntil([this] { return engine->getContent().getStatus() == ContentManager::Status::Failed; }));
    EXPECT_FALSE(hasStarted());
    EXPECT_TRUE(engine->getContent().isHoldingStart()) << "The update screen shows the failure and offers to try again.";

    setPacksStatus(0);
    engine->getContent().retry();
    ASSERT_TRUE(frameUntil([this] { return hasStarted(); }));
    EXPECT_EQ(getGlobalText("greeting"), "Play");
}

TEST_F(ContentStartTest, ExpandsTheGroupsOfItsRulesOnceTheGroupManifestDownloads) {
    rebuild(R"("content": {)" + getServer() + R"(, "groups": "groups.json", "delivery": [{"group": "forest", "mode": "onDemand"}]})");
    run(shipWithoutContent());

    EXPECT_FALSE(hasStarted());
    ASSERT_TRUE(frameUntil([this] { return hasStarted(); })) << (engine->getError() != nullptr ? engine->getError()->getMessage() : "");
    EXPECT_TRUE(engine->getContent().isLocal("data/config.json"));
    EXPECT_FALSE(engine->getContent().isLocal("levels/forest.json")) << "The group delivers the level on demand, so the start never downloaded it.";
}

} // namespace haylen::content
