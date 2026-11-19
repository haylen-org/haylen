#include <gtest/gtest.h>
#include <lua.hpp>

#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "content/UpdateScreen.hpp"
#include "haylen/assets/Manager.hpp"
#include "haylen/content/ContentManager.hpp"
#include "haylen/core/AppConfig.hpp"
#include "haylen/core/Connection.hpp"
#include "haylen/core/Engine.hpp"
#include "haylen/core/EventBus.hpp"
#include "haylen/core/LifecycleEvent.hpp"
#include "haylen/core/Log.hpp"
#include "haylen/core/ScopedConnection.hpp"
#include "haylen/io/MemoryPackage.hpp"
#include "haylen/lua/Application.hpp"
#include "haylen/lua/Error.hpp"
#include "haylen/platform/Event.hpp"
#include "platform/headless/HeadlessHost.hpp"
#include "support/ContentAppFixture.hpp"
#include "support/TemporaryDirectory.hpp"
#include "support/TestFiles.hpp"

namespace haylen::content {

class ContentManagerTest : public test::ContentAppFixture {};

TEST_F(ContentManagerTest, StartsAtOnceWhenTheRequiredContentShipsWithTheApp) {
    run(ship());
    ASSERT_TRUE(frameUntil([this] { return hasStarted(); }));
    EXPECT_EQ(engine->getContent().getProgress().totalBytes, 0U) << "Nothing downloads when the required content ships with the app.";
    EXPECT_TRUE(engine->getContent().isProtected());
    EXPECT_EQ(engine->getContent().getGeneration(), 1U);
    EXPECT_TRUE(engine->getContent().isLocal("data/config.json"));
    EXPECT_FALSE(engine->getContent().isLocal("levels/forest.json"));
}

TEST_F(ContentManagerTest, HoldsTheStartUntilTheRequiredContentDownloads) {
    run(shipWithoutContent());

    EXPECT_FALSE(hasStarted());
    EXPECT_TRUE(engine->getContent().isHoldingStart());
    ASSERT_TRUE(frameUntil([this] { return hasStarted(); }));
    EXPECT_FALSE(engine->getContent().isHoldingStart());
    EXPECT_EQ(engine->getAssets().text("data/config.json"), R"({"level": 1})");
}

TEST_F(ContentManagerTest, ShowsWhyTheRequiredContentCannotDownloadAndTriesAgain) {
    setPacksStatus(503);
    run(shipWithoutContent());

    ASSERT_TRUE(frameUntil([this] { return engine->getContent().getStatus() == ContentManager::Status::Failed; }));
    EXPECT_FALSE(hasStarted());
    ASSERT_TRUE(engine->getContent().getError());
    EXPECT_TRUE(engine->getContent().getError()->isRetryable());

    // The update screen offers to try again, which the keyboard, a gamepad and a TV remote press with the focus on its first button.
    setPacksStatus(0);
    engine->handleEvent({.type = platform::Event::Type::KeyDown, .key = input::Key::Enter});
    EXPECT_EQ(engine->getContent().getStatus(), ContentManager::Status::Downloading);
    ASSERT_TRUE(frameUntil([this] { return hasStarted(); }));
}

// A device without room for the content that the app needs to start says how much room it needs before the download fills it.
TEST_F(ContentManagerTest, RefusesToDownloadTheRequiredContentWithoutRoomForIt) {
    storeRoom = 1024;
    run(shipWithoutContent());

    ASSERT_TRUE(frameUntil([this] { return engine->getContent().getStatus() == ContentManager::Status::Failed; }));
    const std::optional<Error>& error = engine->getContent().getError();
    ASSERT_TRUE(error);
    EXPECT_EQ(error->getCode(), Error::Code::InsufficientStorage) << error->what();
    ASSERT_TRUE(error->getDetails().requiredBytes);

    const UpdateScreen screen(*engine);
    const std::string expected = "The device needs " + UpdateScreen::formatBytes(*error->getDetails().requiredBytes) + " of free room";
    EXPECT_NE(screen.getStatus().find(expected), std::string::npos) << screen.getStatus();
}

// A device that refused the content store leaves the app without updates, and a check says so instead of asking for a server that the app already names.
TEST_F(ContentManagerTest, TellsThatAnAppWithoutItsStoreHasNoUpdates) {
    storeRefused = true;
    run(ship());
    ASSERT_TRUE(frameUntil([this] { return hasStarted(); }));

    try {
        engine->getContent().check([](std::optional<ContentManager::Update>, std::optional<Error>) { ADD_FAILURE() << "A check that cannot start never answers."; });
        FAIL() << "A check without the store raises.";
    } catch (const std::runtime_error& error) {
        EXPECT_STREQ(error.what(), "The content store of the app could not be opened on this device, so it has no updates. The log of the app tells why.");
    }
}

TEST_F(ContentManagerTest, LoadsAnAssetOnDemandOnlyAsynchronously) {
    run(ship());
    engine->frame(1.0 / 60.0);

    try {
        (void)engine->getAssets().json("levels/forest.json");
        FAIL() << "A synchronous load never waits for a download.";
    } catch (const std::runtime_error& error) {
        EXPECT_NE(std::string(error.what()).find("loadAsync"), std::string::npos) << error.what();
    }

    std::shared_ptr<void> loaded;
    std::string failure;
    // clang-format off
    engine->getAssets().loadAsync("json", "levels/forest.json", [&loaded, &failure](std::shared_ptr<void> asset, std::string error) {
        loaded = std::move(asset);
        failure = std::move(error);
    });
    // clang-format on
    ASSERT_TRUE(frameUntil([&loaded, &failure] { return loaded || !failure.empty(); })) << failure;
    ASSERT_TRUE(loaded) << failure;
    EXPECT_EQ((*std::static_pointer_cast<core::Json>(loaded))["trees"], 12);
    EXPECT_TRUE(engine->getContent().isLocal("levels/forest.json"));
    EXPECT_EQ(engine->getAssets().json("levels/forest.json")["trees"], 12);

    // The statistics count the reads of the content and the downloads that brought the level and the music it prefetches.
    ASSERT_TRUE(frameUntil([this] { return engine->getContent().getStatistics().pendingDownloads == 0; }));
    const ContentManager::Statistics statistics = engine->getContent().getStatistics();
    EXPECT_GE(statistics.openShards, 1U);
    EXPECT_GE(statistics.decodedChunks, 2U);
    EXPECT_GT(statistics.decodedBytes, 0U);
    EXPECT_GT(statistics.decodeMilliseconds, 0.0);
    EXPECT_EQ(statistics.activeReads, 0U);
    EXPECT_GT(statistics.downloadedBytes, 0U);
    EXPECT_EQ(statistics.wantedRecords, 0U);
    EXPECT_EQ(statistics.activeTransfers, 0U);
}

TEST_F(ContentManagerTest, PreloadsAGroupWhoseAssetsDownload) {
    run(ship());
    engine->frame(1.0 / 60.0);
    engine->getAssets().defineGroups(core::Json::parse(R"({"groups": {"forest": ["levels/", "data/config.json"]}})"));

    std::vector<float> fractions;
    std::optional<std::vector<std::string>> errors;
    engine->getAssets().preload("forest", [&fractions](float fraction) { fractions.push_back(fraction); }, [&errors](std::vector<std::string> failed) { errors = std::move(failed); });
    ASSERT_TRUE(frameUntil([&errors] { return errors.has_value(); }));

    EXPECT_TRUE(errors->empty());
    ASSERT_FALSE(fractions.empty());
    EXPECT_FLOAT_EQ(fractions.back(), 1.0F);
    EXPECT_TRUE(std::ranges::is_sorted(fractions));
    EXPECT_TRUE(engine->getContent().isLocal("levels/forest.json"));
}

TEST_F(ContentManagerTest, DownloadsPrefetchedContentInTheBackground) {
    run(ship());
    ASSERT_TRUE(frameUntil([this] { return engine->getContent().isLocal("music/theme.ogg"); }));
    EXPECT_FALSE(engine->getContent().isLocal("levels/forest.json"));
    EXPECT_EQ(engine->getAssets().bytes("music/theme.ogg"), game.at("content/music/theme.ogg"));
}

TEST_F(ContentManagerTest, TriesTheRequiredContentAgainWhenTheNetworkComesBack) {
    setPacksStatus(503);
    run(shipWithoutContent());
    ASSERT_TRUE(frameUntil([this] { return engine->getContent().getStatus() == ContentManager::Status::Failed; }));

    setPacksStatus(0);
    engine->handleEvent({.type = platform::Event::Type::NetworkChanged, .online = false});
    engine->handleEvent({.type = platform::Event::Type::NetworkChanged, .online = true});
    EXPECT_EQ(engine->getContent().getStatus(), ContentManager::Status::Downloading);
    ASSERT_TRUE(frameUntil([this] { return hasStarted(); }));
}

TEST_F(ContentManagerTest, StopsPrefetchingInTheBackgroundAndGoesOnOnceActive) {
    server.setStalls(true);
    run(ship());
    ASSERT_TRUE(frameUntil([this] { return engine->getContent().getStatistics().pendingDownloads == 1; }));

    engine->handleEvent({.type = platform::Event::Type::Suspended});
    EXPECT_EQ(engine->getContent().getStatistics().pendingDownloads, 0U) << "The background downloads stop with the app.";
    EXPECT_FALSE(engine->getContent().isLocal("music/theme.ogg"));

    server.setStalls(false);
    engine->handleEvent({.type = platform::Event::Type::Resumed});
    ASSERT_TRUE(frameUntil([this] { return engine->getContent().isLocal("music/theme.ogg"); }));
}

TEST_F(ContentManagerTest, CountsTheShardsAndChunksThatTheDeviceHoldsAndLacks) {
    server.setStalls(true);
    run(ship());
    ContentManager::Statistics statistics;
    // clang-format off
    ASSERT_TRUE(frameUntil([this, &statistics] {
        statistics = engine->getContent().getStatistics();
        return statistics.installedShards > 0;
    }));
    // clang-format on
    EXPECT_GT(statistics.remoteShards, 0U) << "The shards of the level and the music stay with the server.";
    EXPECT_GE(statistics.missingChunks, 2U) << "The level and the music are not on the device.";
    EXPECT_GE(statistics.pendingPrefetch, 1U);
    EXPECT_LT(statistics.pendingPrefetch, statistics.missingChunks) << "The level loads on demand, so only the music is prefetched.";

    server.setStalls(false);
    // clang-format off
    ASSERT_TRUE(frameUntil([this, &statistics] {
        statistics = engine->getContent().getStatistics();
        return statistics.pendingPrefetch == 0;
    }));
    // clang-format on
    EXPECT_TRUE(engine->getContent().isLocal("music/theme.ogg"));
    EXPECT_GE(statistics.missingChunks, 1U) << "The level still waits for the app to load it.";
}

// Apple systems such as iOS and tvOS may empty the caches of an app while it is suspended, so the content that the app needs comes down again once it is active.
TEST_F(ContentManagerTest, BringsBackTheContentThatTheDeviceRemovedInTheBackground) {
    run(shipWithoutContent());
    // The music that the app prefetches is on the device too, so only what the engine brings back downloads after the app resumes.
    ASSERT_TRUE(frameUntil([this] { return hasStarted() && engine->getContent().isLocal("music/theme.ogg") && engine->getContent().getStatistics().pendingDownloads == 0; }));
    ASSERT_TRUE(engine->getContent().isLocal("data/config.json"));

    engine->handleEvent({.type = platform::Event::Type::Suspended});
    std::filesystem::remove_all(storeFolder.getPath() / "segments");
    server.clearExchanges();
    engine->handleEvent({.type = platform::Event::Type::Resumed});
    // clang-format off
    ASSERT_TRUE(frameUntil([this] {
        return !server.getExchanges().empty() && engine->getContent().getStatistics().pendingDownloads == 0 && engine->getContent().isLocal("data/config.json");
    })) << "The removed content downloads again.";
    // clang-format on
    EXPECT_TRUE(std::filesystem::is_directory(storeFolder.getPath() / "segments")) << "The content that came back is in the folder of the store again.";
    EXPECT_EQ(engine->getAssets().json("data/config.json")["level"], 1);
    EXPECT_EQ(engine->getContent().getStatus(), ContentManager::Status::Idle);
}

TEST_F(ContentManagerTest, RefusesToBringAnAssetWithoutRoomForIt) {
    storeRoom = 1024;
    run(ship());
    engine->frame(1.0 / 60.0);

    std::string failure;
    bool loaded = false;
    // clang-format off
    engine->getAssets().loadAsync("json", "levels/forest.json", [&loaded, &failure](std::shared_ptr<void> asset, std::string error) {
        loaded = asset != nullptr;
        failure = std::move(error);
    });
    // clang-format on
    ASSERT_TRUE(frameUntil([&loaded, &failure] { return loaded || !failure.empty(); }));
    EXPECT_FALSE(loaded);
    EXPECT_NE(failure.find("1024 are free"), std::string::npos) << failure;
    EXPECT_FALSE(engine->getContent().isLocal("levels/forest.json"));
}

TEST_F(ContentManagerTest, LogsAPrefetchThatFailsWhileNothingAwaitsItAndKeepsTheAppRunning) {
    run(ship());
    ASSERT_TRUE(frameUntil([this] { return hasStarted(); }));
    setPacksStatus(503);

    std::vector<std::string> lines;
    // clang-format off
    const std::uint64_t listener = core::Log::addListener([&lines](core::Log::Level level, std::string_view line) {
        if (level == core::Log::Level::Warning && line.find("ahead of time") != std::string_view::npos) {
            lines.emplace_back(line);
        }
    });
    // clang-format on
    lua_State* L = engine->getLuaState();
    ASSERT_EQ(luaL_dostring(L, "require('haylen.content').prefetch({'levels/forest.json'})"), LUA_OK) << lua_tostring(L, -1);
    const bool logged = frameUntil([&lines] { return !lines.empty(); });

    for (int frame = 0; frame < 30; ++frame) {
        engine->frame(1.0 / 60.0);
    }

    core::Log::removeListener(listener);
    ASSERT_TRUE(logged);
    ASSERT_EQ(lines.size(), 1U);
    EXPECT_NE(lines.front().find("The asset \"levels/forest.json\" did not reach the device ahead of time: "), std::string::npos) << lines.front();
    EXPECT_NE(lines.front().find("503"), std::string::npos) << lines.front();
    EXPECT_EQ(engine->getError(), nullptr) << "A prefetch that nothing awaits never stops the app.";
}

TEST_F(ContentManagerTest, RejectsAPrefetchWithTheFailureThatDescribesIt) {
    run(ship());
    ASSERT_TRUE(frameUntil([this] { return hasStarted(); }));
    setPacksStatus(503);

    lua_State* L = engine->getLuaState();
    // clang-format off
    ASSERT_EQ(luaL_dostring(L, R"lua(
        require('async').spawn(function()
            local done, message, failure = require('haylen.content').prefetch({'levels/forest.json'}):await()
            prefetched = {done = done, message = message, code = failure.code, status = failure.httpStatus, retryable = failure.retryable, pack = failure.packId, record = failure.storedId, error = failure.error}
        end)
    )lua"), LUA_OK) << lua_tostring(L, -1);
    ASSERT_TRUE(frameUntil([L] {
        lua_getglobal(L, "prefetched");
        const bool settled = lua_istable(L, -1);
        lua_pop(L, 1);
        return settled;
    }));

    lua_getglobal(L, "prefetched");
    const auto field = [L](const char* name) {
        lua_getfield(L, -1, name);
        std::string value = lua_isstring(L, -1) != 0 ? lua_tostring(L, -1) : (lua_isboolean(L, -1) ? (lua_toboolean(L, -1) != 0 ? "true" : "false") : "nil");
        lua_pop(L, 1);
        return value;
    };
    // clang-format on
    EXPECT_EQ(field("done"), "nil");
    EXPECT_EQ(field("code"), "unexpectedHttpStatus");
    EXPECT_EQ(field("status"), "503");
    EXPECT_EQ(field("retryable"), "true");
    EXPECT_EQ(field("pack").size(), Digest::kSize * 2);
    EXPECT_EQ(field("record").size(), Digest::kSize * 2);
    EXPECT_EQ(field("error"), field("message"));
    lua_pop(L, 1);
}

TEST_F(ContentManagerTest, UpdatesFromTheChannelAndAppliesTheUpdateOnRestart) {
    Files update = game;
    update["content/data/config.json"] = test::TestFiles::bytes(R"({"level": 2})");
    publish(update);
    run(ship());
    engine->frame(1.0 / 60.0);

    std::optional<std::string> ready;
    const core::ScopedConnection listener(engine->getEvents().on(core::LifecycleEvent::kContentUpdateReady, [&ready](core::EventBus::Event& event) { ready = event.getData().dump(); }));
    std::optional<std::optional<Error>> downloaded;
    engine->getContent().download({}, [&downloaded](std::optional<Error> error) { downloaded = std::move(error); });
    ASSERT_TRUE(frameUntil([&downloaded] { return downloaded.has_value(); }));
    ASSERT_FALSE(*downloaded) << (*downloaded)->what();
    engine->frame(1.0 / 60.0);

    EXPECT_TRUE(ready);
    EXPECT_EQ(engine->getContent().getStatus(), ContentManager::Status::Ready);
    EXPECT_EQ(engine->getAssets().text("data/config.json"), R"({"level": 1})");
    engine->getContent().apply();
    EXPECT_TRUE(engine->isRestartRequested());

    run(ship());
    engine->frame(1.0 / 60.0);
    EXPECT_EQ(engine->getContent().getGeneration(), 2U);
    EXPECT_EQ(engine->getAssets().text("data/config.json"), R"({"level": 2})");
}

TEST_F(ContentManagerTest, PlaysTheContentItShipsWhenADownloadedUpdateCannotComeBack) {
    Files update = game;
    update["content/data/config.json"] = test::TestFiles::bytes(R"({"level": 2})");
    publish(update);
    run(ship());
    engine->frame(1.0 / 60.0);
    std::optional<std::optional<Error>> downloaded;
    engine->getContent().download({}, [&downloaded](std::optional<Error> error) { downloaded = std::move(error); });
    ASSERT_TRUE(frameUntil([&downloaded] { return downloaded.has_value(); }));
    engine.reset();

    // The system cleared the downloads and the server is gone, so the update cannot come back.
    std::filesystem::remove_all(storeFolder.getPath() / "segments");
    setPacksStatus(404);
    run(ship());
    ASSERT_TRUE(frameUntil([this] { return hasStarted(); }));
    EXPECT_EQ(engine->getAssets().text("data/config.json"), R"({"level": 1})");
}

TEST_F(ContentManagerTest, TellsTheAppThatItPlaysEarlierContentWhenTheUpdateWasDamaged) {
    Files update = game;
    update["content/data/config.json"] = test::TestFiles::bytes(R"({"level": 2})");
    publish(update);
    run(ship());
    engine->frame(1.0 / 60.0);
    std::optional<std::optional<Error>> downloaded;
    engine->getContent().download({}, [&downloaded](std::optional<Error> error) { downloaded = std::move(error); });
    ASSERT_TRUE(frameUntil([&downloaded] { return downloaded.has_value(); }));
    engine.reset();

    // The device lost the manifest of the update, so the app falls back to the content it ships and its main module hears why.
    for (const std::filesystem::directory_entry& entry : std::filesystem::directory_iterator(storeFolder.getPath() / "manifests")) {
        std::filesystem::remove(entry.path());
    }

    run(ship());
    ASSERT_TRUE(frameUntil([this] { return hasStarted(); }));
    engine->frame(1.0 / 60.0);
    EXPECT_EQ(engine->getContent().getGeneration(), 1U);
    EXPECT_FALSE(getGlobalText("recovered").empty());
}

TEST(ContentDevelopmentTest, KeepsTheFilesThatDownloadOffTheDeviceUntilSomethingBringsThem) {
    test::TemporaryDirectory data;
    platform::HeadlessHost host(data.getPath());
    std::map<std::string, std::vector<std::uint8_t>> files;
    files.emplace("haylen.json", test::TestFiles::bytes(R"({"content": {"delivery": [{"match": "levels/**", "mode": "onDemand"}]}})"));
    files.emplace("source/main.lua", test::TestFiles::bytes("started = true"));
    files.emplace("content/levels/forest.json", test::TestFiles::bytes(R"({"trees": 12})"));
    files.emplace("content/data/config.json", test::TestFiles::bytes(R"({"level": 1})"));
    auto package = std::make_shared<io::MemoryPackage>("development", std::move(files));
    core::AppConfig config = core::AppConfig::fromPackage(*package);
    auto application = std::make_unique<lua::Application>();
    application->configure(config);
    core::Engine engine(host, package, std::move(config), std::move(application));
    engine.start();
    engine.frame(1.0 / 60.0);

    ContentManager& content = engine.getContent();
    EXPECT_FALSE(content.isProtected());
    EXPECT_EQ(content.getGeneration(), 0U);
    EXPECT_TRUE(content.isLocal("data/config.json"));
    EXPECT_FALSE(content.isLocal("levels/forest.json"));
    EXPECT_THROW((void)engine.getAssets().json("levels/forest.json"), std::runtime_error);
    EXPECT_THROW(content.check([](std::optional<ContentManager::Update>, std::optional<Error>) {}), std::logic_error);

    bool brought = false;
    content.acquire({"levels/forest.json"}, {}, [&brought](std::optional<Error> error) { brought = !error; });
    engine.frame(1.0 / 60.0);
    EXPECT_TRUE(brought);
    EXPECT_EQ(engine.getAssets().json("levels/forest.json")["trees"], 12);
}

// Development plays the folder of the app with the delivery of a release, so languages that a rule delivers later hold the start until they are brought.
TEST(ContentDevelopmentTest, HoldsTheStartForTheLanguagesThatARuleDeliversLater) {
    test::TemporaryDirectory data;
    platform::HeadlessHost host(data.getPath());
    std::map<std::string, std::vector<std::uint8_t>> files;
    files.emplace("haylen.json", test::TestFiles::bytes(R"({"localization": {"folder": "locale"}, "content": {"delivery": [{"match": "locale/**", "mode": "onDemand"}]}})"));
    files.emplace("source/main.lua", test::TestFiles::bytes("greeting = require('haylen.localization').text('menu.play')"));
    files.emplace("content/locale/en.json", test::TestFiles::bytes(R"({"menu": {"play": "Play"}})"));
    auto package = std::make_shared<io::MemoryPackage>("development", std::move(files));
    core::AppConfig config = core::AppConfig::fromPackage(*package);
    auto application = std::make_unique<lua::Application>();
    application->configure(config);
    core::Engine engine(host, package, std::move(config), std::move(application));
    engine.start();

    EXPECT_TRUE(engine.isStartHeld());
    engine.frame(1.0 / 60.0);
    engine.frame(1.0 / 60.0);
    lua_State* L = engine.getLuaState();
    lua_getglobal(L, "greeting");
    EXPECT_STREQ(lua_tostring(L, -1), "Play");
    lua_pop(L, 1);
}

} // namespace haylen::content
