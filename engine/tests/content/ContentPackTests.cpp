#include <gtest/gtest.h>
#include <lua.hpp>

#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "content/ReleaseInspector.hpp"
#include "content/ReleaseStager.hpp"
#include "haylen/assets/Manager.hpp"
#include "haylen/content/ContentManager.hpp"
#include "haylen/core/AppConfig.hpp"
#include "haylen/core/Connection.hpp"
#include "haylen/core/Engine.hpp"
#include "haylen/core/Json.hpp"
#include "haylen/io/MemoryPackage.hpp"
#include "haylen/lua/Stack.hpp"
#include "io/CompositePackage.hpp"
#include "support/ContentAppFixture.hpp"
#include "support/StorePackSource.hpp"
#include "support/TestFiles.hpp"

namespace haylen::content {

// An app without a server whose expansion is a pack that a store installs next to the app, or leaves out for a player who does not own it.
class ContentPackTest : public test::ContentAppFixture {
  protected:
    ContentPackTest() {
        game.erase("content/levels/forest.json");
        const std::string app = R"({"name": "Packs", "identifier": "com.haylen.tests", "content": {"delivery": [{"match": "expansion/**", "pack": "expansion"}]}})";
        game["haylen.json"] = test::TestFiles::bytes(app);
        game.emplace("content/expansion/island.json", test::TestFiles::bytes(R"({"palms": 7})"));
        release.build(io::MemoryPackage("source", game));
        const ReleaseInspector inspector(release.getKeys(), {release.getSigningKey().getVerifyingKey()});
        const core::AppConfig config = core::AppConfig::fromJson(core::Json::parse(app));
        (void)ReleaseStager(inspector, config.content).stage(release.getFolder(), shipped.getPath() / "app", std::nullopt, shipped.getPath() / "packs", ReleaseStager::Content::All);
    }

    // Loads an asset of the expansion and returns the error of the load, empty once the asset loaded.
    [[nodiscard]] std::string loadIsland() {
        std::optional<std::string> failure;
        engine->getAssets().loadAsync("json", "expansion/island.json", [&failure](const std::shared_ptr<void>&, std::string error) { failure = std::move(error); });
        if (!frameUntil([&failure] { return failure.has_value(); })) {
            throw std::runtime_error("The load of the island never ended.");
        }

        return *failure;
    }
};

TEST_F(ContentPackTest, AsksItsSourcesForAPackThatAnAssetNeeds) {
    run(shipped.getPath() / "app");
    ASSERT_TRUE(frameUntil([this] { return hasStarted(); }));
    const auto other = std::make_shared<test::StorePackSource>();
    const auto store = std::make_shared<test::StorePackSource>();
    store->install("expansion", shipped.getPath() / "packs" / "expansion");
    const core::Connection first = engine->getContent().addPackSource(other);
    const core::Connection second = engine->getContent().addPackSource(store);

    // The load never names the store: the source that delivers nothing passes the pack on to the next one, which installs it.
    EXPECT_EQ(loadIsland(), "");
    EXPECT_EQ(other->getRequests(), 1);
    EXPECT_EQ(store->getRequests(), 1);
    EXPECT_TRUE(engine->getContent().isLocal("expansion/island.json"));
    EXPECT_EQ(engine->getAssets().json("expansion/island.json")["palms"], 7);
}

TEST_F(ContentPackTest, FailsALoadWhoseSourceFailsOrGoesAway) {
    run(shipped.getPath() / "app");
    ASSERT_TRUE(frameUntil([this] { return hasStarted(); }));
    const auto broken = std::make_shared<test::StorePackSource>();
    broken->fail("The store is offline.");
    core::Connection connection = engine->getContent().addPackSource(broken);
    EXPECT_EQ(loadIsland(), "The store is offline.");
    connection.disconnect();
    const auto misplaced = std::make_shared<test::StorePackSource>();
    misplaced->install("expansion", shipped.getPath() / "missing");
    core::Connection elsewhere = engine->getContent().addPackSource(misplaced);
    EXPECT_NE(loadIsland().find("which is no folder"), std::string::npos);
    elsewhere.disconnect();

    // A source that its connection removes while the store downloads counts as one that delivers nothing, and its late answer changes nothing.
    const auto slow = std::make_shared<test::StorePackSource>();
    slow->install("expansion", shipped.getPath() / "packs" / "expansion");
    slow->hold();
    core::Connection waiting = engine->getContent().addPackSource(slow);
    std::optional<std::string> failure;
    engine->getAssets().loadAsync("json", "expansion/island.json", [&failure](const std::shared_ptr<void>&, std::string error) { failure = std::move(error); });
    ASSERT_TRUE(frameUntil([&slow] { return slow->getRequests() == 1; }));
    waiting.disconnect();
    ASSERT_TRUE(frameUntil([&failure] { return failure.has_value(); }));
    EXPECT_NE(failure->find("no source of packs delivered the pack"), std::string::npos) << *failure;
    slow->answer();
    engine->frame(1.0 / 60.0);
    EXPECT_FALSE(engine->getContent().isLocal("expansion/island.json"));
}

TEST_F(ContentPackTest, TakesPacksFromTheSourcesThatLuaGives) {
    run(shipped.getPath() / "app");
    ASSERT_TRUE(frameUntil([this] { return hasStarted(); }));
    lua_State* L = engine->getLuaState();
    lua::Stack::push(L, (shipped.getPath() / "packs" / "expansion").string());
    lua_setglobal(L, "packFolder");

    // A source that returns something else than a folder and one whose promise rejects fail the load, while a source that answers at once with nothing passes the pack on to one that waits for the store itself and then in a promise, so a yield passes through both continuations of the source.
    // clang-format off
    ASSERT_EQ(luaL_dostring(L, R"lua(
        local content = require('haylen.content')
        local async = require('async')
        local load = function()
            local island, message = require('haylen.assets').loadAsync('expansion/island.json', 'json'):await()
            return island and island.palms or message
        end
        async.spawn(function()
            local wrong = content.addPackSource(function() return 42 end)
            refusedValue = load()
            wrong:disconnect()
            local offline = content.addPackSource(function() return async.promise(function() error('The store is offline.', 0) end) end)
            refusedPromise = load()
            offline:disconnect()
            content.addPackSource(function(pack) skipped = pack end)
            content.addPackSource(function(pack, progress)
                progress({receivedBytes = 5, totalBytes = 10})
                async.sleep(5):await()
                return async.promise(function()
                    async.sleep(10):await()
                    return packFolder
                end)
            end)
            loaded = load()
        end)
    )lua"), LUA_OK) << lua_tostring(L, -1);
    ASSERT_TRUE(frameUntil([L] {
        lua_getglobal(L, "loaded");
        const bool settled = !lua_isnil(L, -1);
        lua_pop(L, 1);
        return settled;
    }));
    const auto global = [L](const char* name) {
        lua_getglobal(L, name);
        std::string value = luaL_tolstring(L, -1, nullptr);
        lua_pop(L, 2);
        return value;
    };
    // clang-format on
    EXPECT_NE(global("refusedValue").find("must return the folder of the pack"), std::string::npos) << global("refusedValue");
    EXPECT_NE(global("refusedPromise").find("failed to deliver the pack \"expansion\". The store is offline."), std::string::npos) << global("refusedPromise");
    EXPECT_EQ(global("skipped"), "expansion");
    EXPECT_EQ(global("loaded"), "7");
    EXPECT_TRUE(engine->getContent().isLocal("expansion/island.json"));
}

TEST_F(ContentPackTest, StartsWithoutAPackThatIsNotInstalledAndReadsOneThatArrives) {
    run(shipped.getPath() / "app");
    ASSERT_TRUE(frameUntil([this] { return hasStarted(); })) << "A pack never holds back the start.";
    EXPECT_FALSE(engine->getContent().isLocal("expansion/island.json"));
    std::string failure;
    engine->getAssets().loadAsync("json", "expansion/island.json", [&failure](const std::shared_ptr<void>&, std::string error) { failure = std::move(error); });
    ASSERT_TRUE(frameUntil([&failure] { return !failure.empty(); }));
    EXPECT_NE(failure.find("no server"), std::string::npos) << failure;

    // A store delivers the pack while the app runs, and its assets load from the folder it installed them into.
    EXPECT_THROW(engine->getContent().addPackFolder(shipped.getPath() / "missing"), std::runtime_error);
    engine->getContent().addPackFolder(shipped.getPath() / "packs" / "expansion");
    EXPECT_TRUE(engine->getContent().isLocal("expansion/island.json"));
    EXPECT_EQ(engine->getAssets().json("expansion/island.json")["palms"], 7);

    // The store installs the pack into the folder of packs next to the app, which the runtime reads with the release.
    auto files = std::make_shared<io::CompositePackage>("app", std::vector<std::shared_ptr<const io::Package>>{io::Package::openDirectory(shipped.getPath() / "app"), io::Package::openDirectory(shipped.getPath() / "packs" / "expansion")});
    run(files);
    ASSERT_TRUE(frameUntil([this] { return hasStarted(); }));
    EXPECT_TRUE(engine->getContent().isLocal("expansion/island.json"));
    EXPECT_EQ(engine->getAssets().json("expansion/island.json")["palms"], 7);
}

} // namespace haylen::content
