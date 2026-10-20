#include <gtest/gtest.h>
#include <lua.hpp>

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "content/ChannelPublisher.hpp"
#include "content/FolderStoreFiles.hpp"
#include "content/PublicationLayout.hpp"
#include "content/ReleaseInspector.hpp"
#include "content/ReleasePackage.hpp"
#include "content/ReleaseStager.hpp"
#include "haylen/assets/Manager.hpp"
#include "haylen/content/ContentManager.hpp"
#include "haylen/core/AppConfig.hpp"
#include "haylen/core/Connection.hpp"
#include "haylen/core/Engine.hpp"
#include "haylen/core/EventBus.hpp"
#include "haylen/core/LifecycleEvent.hpp"
#include "haylen/core/ScopedConnection.hpp"
#include "haylen/io/MemoryPackage.hpp"
#include "haylen/lua/Application.hpp"
#include "haylen/lua/Stack.hpp"
#include "haylen/platform/Event.hpp"
#include "io/CompositePackage.hpp"
#include "platform/headless/HeadlessHost.hpp"
#include "support/ContentServer.hpp"
#include "support/LimitedStoreFiles.hpp"
#include "support/ReleaseFixture.hpp"
#include "support/StorePackSource.hpp"
#include "support/TemporaryDirectory.hpp"
#include "support/TestFiles.hpp"

namespace haylen::content {

// An app that runs a protected release with a content server: the required content that the app waits for, the files that download in the background or when the app asks for them, and the updates of its channel, all served by a local server.
class ContentManagerTest : public ::testing::Test {
  protected:
    using Files = std::map<std::string, std::vector<std::uint8_t>>;

    ContentManagerTest() {
        game = makeGame();
        release.build(io::MemoryPackage("source", game));
        publish(game);
    }

    [[nodiscard]] Files makeGame() const {
        const std::string app = R"({"name": "Content", "identifier": "com.haylen.tests", "content": {"updates": {"baseUrl": ")" + server.getUrl() + R"(", "enabled": false}, "delivery": [{"match": "levels/**", "mode": "onDemand"}, {"match": "music/**", "mode": "prefetch"}]}})";
        Files files;
        files.emplace("app.json", test::TestFiles::bytes(app));
        files.emplace("source/main.lua", test::TestFiles::bytes("started = true\nrequire('haylen.events').on('contentRecovered', function(event) recovered = event.reason end)"));
        files.emplace("content/data/config.json", test::TestFiles::bytes(R"({"level": 1})"));
        files.emplace("content/levels/forest.json", test::TestFiles::bytes(R"({"trees": 12})"));
        files.emplace("content/music/theme.ogg", test::TestFiles::randomBytes(300 * 1024, 3));
        return files;
    }

    void publish(const Files& files) const {
        const ChannelPublisher publisher(release.getKeys(), release.getContentKeyId(), release.getSharedSigningKey());
        (void)publisher.publish(io::MemoryPackage("source", files), tree.getPath(), {.profile = std::string(test::ReleaseFixture::kProfile), .appBuild = test::ReleaseFixture::kAppBuild, .channel = "stable", .release = release.getFolder()});
    }

    // The release of an app on a platform that holds none of its content, such as a web page or a device that cleared its downloads: only the manifests and the app domain.
    [[nodiscard]] std::filesystem::path shipWithoutContent() {
        const std::filesystem::path folder = shipped.getPath() / std::to_string(++ships);
        std::filesystem::create_directories(folder);
        for (const std::string_view manifest : {ReleasePackage::kAppManifestFile, ReleasePackage::kContentManifestFile}) {
            std::filesystem::copy_file(release.getFolder() / manifest, folder / manifest);
        }
        const ReleaseInspector inspector(release.getKeys(), {release.getSigningKey().getVerifyingKey()});
        for (const ShardReference& shard : inspector.inspect(release.getFolder()).app.envelope.shards) {
            std::filesystem::copy_file(release.getFolder() / shard.getFileName(), folder / shard.getFileName());
        }
        return folder;
    }

    // Makes the server answer every pack with a status, or serve them again with 0.
    void setPacksStatus(int status) {
        for (const std::filesystem::directory_entry& entry : std::filesystem::recursive_directory_iterator(tree.getPath() / PublicationLayout::kPacksFolder)) {
            if (entry.is_regular_file()) {
                server.setStatus(entry.path().lexically_relative(tree.getPath()).generic_string(), status);
            }
        }
    }

    // Lays out the release the way a platform ships it: what the app needs to start, and nothing that downloads.
    [[nodiscard]] std::filesystem::path ship() {
        const std::filesystem::path folder = shipped.getPath() / std::to_string(++ships);
        const ReleaseInspector inspector(release.getKeys(), {release.getSigningKey().getVerifyingKey()});
        (void)ReleaseStager(inspector, {}).stage(release.getFolder(), folder, std::nullopt, std::nullopt);
        return folder;
    }

    void run(const std::filesystem::path& folder) {
        run(io::Package::openDirectory(folder));
    }

    void run(std::shared_ptr<const io::Package> files) {
        engine.reset();
        const VerifyingKey trusted = release.getSigningKey().getVerifyingKey();
        std::unique_ptr<StoreFiles> storeFiles;
        if (storeRoom) {
            storeFiles = std::make_unique<test::LimitedStoreFiles>(storeFolder.getPath(), *storeRoom);
        } else {
            storeFiles = std::make_unique<FolderStoreFiles>(storeFolder.getPath());
        }
        auto store = std::make_shared<LocalStore>(std::move(storeFiles), release.getCompatibility().application, test::ReleaseFixture::kAppBuild);
        package = ReleasePackage::open(std::move(files), release.getKeys(), std::span(&trusted, 1), release.getCompatibility(), std::move(store));
        core::AppConfig config = core::AppConfig::fromPackage(*package);
        auto application = std::make_unique<lua::Application>();
        application->configure(config);
        engine = std::make_unique<core::Engine>(host, package, std::move(config), std::move(application));
        engine->start();
    }

    bool frameUntil(const std::function<bool()>& condition, std::chrono::milliseconds timeout = std::chrono::seconds(20)) {
        const auto deadline = std::chrono::steady_clock::now() + timeout;
        while (!condition()) {
            if (std::chrono::steady_clock::now() > deadline) {
                return false;
            }
            engine->frame(1.0 / 60.0);
        }
        return true;
    }

    [[nodiscard]] bool hasStarted() const {
        lua_State* L = engine->getLuaState();
        lua_getglobal(L, "started");
        const bool started = lua_toboolean(L, -1) != 0;
        lua_pop(L, 1);
        return started;
    }

    [[nodiscard]] std::string getRecovery() const {
        lua_State* L = engine->getLuaState();
        lua_getglobal(L, "recovered");
        std::string reason = lua_isstring(L, -1) != 0 ? lua_tostring(L, -1) : "";
        lua_pop(L, 1);
        return reason;
    }

    test::TemporaryDirectory tree;
    test::ContentServer server{tree.getPath()};
    test::ReleaseFixture release;
    Files game;
    test::TemporaryDirectory shipped;
    test::TemporaryDirectory storeFolder;
    test::TemporaryDirectory data;
    platform::HeadlessHost host{data.getPath()};
    int ships = 0;
    std::optional<std::uint64_t> storeRoom;
    std::shared_ptr<ReleasePackage> package;
    std::unique_ptr<core::Engine> engine;
};

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
    const core::ScopedConnection listener = engine->getEvents().on(core::LifecycleEvent::kContentUpdateReady, [&ready](core::EventBus::Event& event) { ready = event.getData().dump(); });
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
    EXPECT_FALSE(getRecovery().empty());
}

TEST(ContentDevelopmentTest, KeepsTheFilesThatDownloadOffTheDeviceUntilSomethingBringsThem) {
    test::TemporaryDirectory data;
    platform::HeadlessHost host(data.getPath());
    std::map<std::string, std::vector<std::uint8_t>> files;
    files.emplace("app.json", test::TestFiles::bytes(R"({"content": {"delivery": [{"match": "levels/**", "mode": "onDemand"}]}})"));
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

// An app without a server whose expansion is a pack that a store installs next to the app, or leaves out for a player who does not own it.
class ContentPackTest : public ContentManagerTest {
  protected:
    ContentPackTest() {
        game.erase("content/levels/forest.json");
        const std::string app = R"({"name": "Packs", "identifier": "com.haylen.tests", "content": {"delivery": [{"match": "expansion/**", "pack": "expansion"}]}})";
        game["app.json"] = test::TestFiles::bytes(app);
        game.emplace("content/expansion/island.json", test::TestFiles::bytes(R"({"palms": 7})"));
        release.build(io::MemoryPackage("source", game));
        const ReleaseInspector inspector(release.getKeys(), {release.getSigningKey().getVerifyingKey()});
        const core::AppConfig config = core::AppConfig::fromJson(core::Json::parse(app));
        (void)ReleaseStager(inspector, config.content).stage(release.getFolder(), shipped.getPath() / "app", std::nullopt, shipped.getPath() / "packs", ReleaseStager::Content::All);
    }

    std::string loadIsland();
};

// Loads an asset of the expansion and returns the error of the load, empty once the asset loaded.
std::string ContentPackTest::loadIsland() {
    std::optional<std::string> failure;
    engine->getAssets().loadAsync("json", "expansion/island.json", [&failure](const std::shared_ptr<void>&, std::string error) { failure = std::move(error); });
    EXPECT_TRUE(frameUntil([&failure] { return failure.has_value(); }));
    return failure.value_or("The load never ended.");
}

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

    // A source that returns something else than a folder and one whose promise rejects fail the load, while a source that answers at once with nothing passes the pack on to one that waits for the store in a promise.
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
