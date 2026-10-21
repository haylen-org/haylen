#include <gtest/gtest.h>

#include <lua.hpp>

#include <algorithm>
#include <atomic>
#include <map>
#include <memory>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

#include "core/EmbeddedFiles.hpp"
#include "haylen/assets/Manager.hpp"
#include "haylen/core/Connection.hpp"
#include "haylen/core/EventBus.hpp"
#include "haylen/core/LifecycleEvent.hpp"
#include "haylen/platform/Event.hpp"
#include "haylen/plugins/AssetsPlugin.hpp"
#include "support/EngineFixture.hpp"
#include "support/TestFiles.hpp"

namespace haylen::assets {

class AssetsManagerTest : public ::testing::Test {
  protected:
    [[nodiscard]] static std::string getFontFile() {
        const std::span<const std::uint8_t> data = core::EmbeddedFiles::getDefaultFont();
        return {data.begin(), data.end()};
    }

    [[nodiscard]] static std::map<std::string, std::string> getAssetFiles() {
        const std::vector<std::uint8_t> red = test::TestFiles::pngImage(4, 4, 0xFF0000FFU);
        const std::vector<std::uint8_t> blue = test::TestFiles::pngImage(2, 2, 0x0000FFFFU);
        return {
            {"content/images/red.png", std::string(red.begin(), red.end())}, {"content/images/blue.png", std::string(blue.begin(), blue.end())}, {"content/images/readme.txt", "not an asset type"}, {"content/data/config.json", R"({"speed": 3})"}, {"content/data/broken.json", "{"}, {"content/fonts/ui.ttf", getFontFile()}, {"content/preload.json", R"({"groups": {"menu": ["images/", {"path": "data/config.json", "type": "json"}], "broken": ["data/broken.json", "images/missing.png"], "empty": []}})"},
        };
    }
};

TEST_F(AssetsManagerTest, LoadsAndCachesSynchronously) {
    test::EngineFixture fixture(getAssetFiles());
    assets::Manager& assets = fixture.engine().getAssets();

    const graphics::Texture red = assets.texture("images/red.png");
    const graphics::Texture smooth = assets.texture("images/red.png", {.filter = graphics::Texture::Filter::Linear});
    const std::shared_ptr<text::Font> font = assets.font("fonts/ui.ttf");
    EXPECT_EQ(red.getSize(), math::Vec2(4.0F, 4.0F));
    EXPECT_EQ(assets.texture("images/red.png"), red);
    EXPECT_NE(smooth, red);
    EXPECT_EQ(smooth.getOptions().filter, graphics::Texture::Filter::Linear);
    EXPECT_EQ(assets.font("fonts/ui.ttf"), font);
    EXPECT_EQ(assets.getCachedCount(), 3U);

    EXPECT_EQ(assets.json("data/config.json").at("speed"), 3);
    EXPECT_EQ(assets.text("data/config.json"), R"({"speed": 3})");
    EXPECT_EQ(assets.bytes("images/readme.txt").size(), 17U);
    EXPECT_TRUE(assets.exists("images/red.png"));
    EXPECT_FALSE(assets.exists("images/missing.png"));
    EXPECT_EQ(assets.list("images").size(), 3U);

    EXPECT_THROW((void)assets.texture("images/missing.png"), std::runtime_error);
    EXPECT_THROW((void)assets.json("data/broken.json"), core::Json::exception);
    EXPECT_THROW((void)assets.load("model", "images/red.png"), std::invalid_argument);
    EXPECT_THROW((void)assets.load("texture", "images/red.png", {{"filter", "blurry"}}), std::invalid_argument);
    EXPECT_THROW((void)assets.getTypeForPath("images/readme.txt"), std::invalid_argument);
    EXPECT_THROW((void)assets.load("texture", "images/red.png", {{"filter", "linear"}, {"anisotropy", 4}}), std::invalid_argument);
    EXPECT_THROW((void)assets.load("texture", "images/red.png", {{"mipmaps", "yes"}}), std::invalid_argument);
    EXPECT_TRUE(assets.texture("images/red.png", {.filter = graphics::Texture::Filter::Linear, .mipmaps = true}).getOptions().mipmaps);
    EXPECT_THROW((void)assets.load("json", "data/config.json", {{"strict", true}}), std::invalid_argument);
    EXPECT_THROW((void)assets.load("json", "data/config.json", 3), std::invalid_argument);
}

// The manager learns when the last holder of an asset lets go, so a preloaded asset stays loaded until every group that holds it unloads.
TEST_F(AssetsManagerTest, PublishesLoadedUnloadedAndReloadedEvents) {
    test::EngineFixture fixture(getAssetFiles());
    assets::Manager& assets = fixture.engine().getAssets();
    core::EventBus& events = fixture.engine().getEvents();
    std::vector<std::string> log;
    // clang-format off
    const auto record = [&log](std::string kind) {
        return [&log, kind](core::EventBus::Event& event) {
            log.push_back(kind + " " + event.getData().at("type").get<std::string>() + " " + event.getData().at("path").get<std::string>());
        };
    };
    // clang-format on
    const core::Connection loaded = events.on(core::LifecycleEvent::kAssetLoaded, record("loaded"));
    const core::Connection unloaded = events.on(core::LifecycleEvent::kAssetUnloaded, record("unloaded"));
    const core::Connection reloaded = events.on(core::LifecycleEvent::kAssetReloaded, record("reloaded"));

    {
        const graphics::Texture red = assets.texture("./images/red.png");
        const graphics::Texture again = assets.texture("images/red.png");
        EXPECT_TRUE(log.empty()) << "Asset events wait for the end of the frame.";
        fixture.frames(1);
        EXPECT_EQ(log, (std::vector<std::string>{"loaded texture images/red.png"}));

        fixture.package().setFile("content/images/red.png", test::TestFiles::pngImage(8, 8, 0x00FF00FFU));
        EXPECT_EQ(assets.reload("images/red.png"), 1U);
        fixture.frames(1);
        EXPECT_EQ(log.back(), "reloaded texture images/red.png");
        EXPECT_EQ(red.getSize(), math::Vec2(8.0F, 8.0F));
    }
    fixture.frames(1);
    EXPECT_EQ(log.back(), "unloaded texture images/red.png");

    assets.defineGroup("first", {{.path = "images/blue.png"}});
    assets.defineGroup("second", {{.path = "images/blue.png"}});
    int completions = 0;
    assets.preload("first", {}, [&completions](std::vector<std::string>) { ++completions; });
    assets.preload("second", {}, [&completions](std::vector<std::string>) { ++completions; });
    ASSERT_TRUE(fixture.frameUntil([&] { return completions == 2; }));
    fixture.frames(1);
    EXPECT_EQ(log.back(), "loaded texture images/blue.png");

    log.clear();
    assets.unloadGroup("first");
    fixture.frames(1);
    EXPECT_TRUE(log.empty());
    assets.unloadGroup("second");
    fixture.frames(1);
    EXPECT_EQ(log, (std::vector<std::string>{"unloaded texture images/blue.png"}));

    // clang-format off
    fixture.runLua(R"(
        local events = require('haylen.events')
        seen = {}
        events.on('assetLoaded', function(data) seen[#seen + 1] = 'loaded ' .. data.type .. ' ' .. data.path end)
        events.on('assetUnloaded', function(data) seen[#seen + 1] = 'unloaded ' .. data.path end)
        local font = require('haylen.assets').load('fonts/ui.ttf')
        font = nil
        collectgarbage() collectgarbage()
    )");
    // clang-format on
    fixture.frames(1);
    EXPECT_EQ(fixture.lua("return table.concat(seen, ', ')"), "loaded font fonts/ui.ttf, unloaded fonts/ui.ttf");
}

TEST_F(AssetsManagerTest, EqualOptionsShareOneCachedAsset) {
    test::EngineFixture fixture(getAssetFiles());
    assets::Manager& assets = fixture.engine().getAssets();

    const graphics::Texture preloaded(std::static_pointer_cast<graphics::TextureResource>(assets.load("texture", "images/red.png")));
    EXPECT_EQ(assets.texture("images/red.png"), preloaded);
    EXPECT_EQ(assets.texture("images/red.png", {.filter = graphics::Texture::Filter::Nearest, .wrap = graphics::Texture::Wrap::Clamp}), preloaded);
    EXPECT_EQ(graphics::Texture(std::static_pointer_cast<graphics::TextureResource>(assets.load("texture", "./images/red.png", {{"wrap", "clamp"}}))), preloaded);
    const std::shared_ptr<text::Font> font = assets.font("fonts/ui.ttf");
    EXPECT_EQ(std::static_pointer_cast<text::Font>(assets.load("font", "fonts/ui.ttf", core::Json::object())), font);
    EXPECT_EQ(assets.getCachedCount(), 2U);
}

TEST_F(AssetsManagerTest, ReleasesUnusedEntries) {
    test::EngineFixture fixture(getAssetFiles());
    assets::Manager& assets = fixture.engine().getAssets();
    {
        const graphics::Texture temporary = assets.texture("images/blue.png");
        EXPECT_TRUE(temporary.isValid());
    }
    EXPECT_GE(assets.releaseUnused(), 1U);
    EXPECT_EQ(assets.releaseUnused(), 0U);
}

TEST_F(AssetsManagerTest, LoadsAsynchronouslyOnWorkers) {
    test::EngineFixture fixture(getAssetFiles());
    assets::Manager& assets = fixture.engine().getAssets();

    graphics::Texture first;
    graphics::Texture second;
    std::string error;
    int callbacks = 0;
    // clang-format off
    assets.textureAsync("images/red.png", [&](graphics::Texture texture, std::string) {
        first = texture;
        ++callbacks;
    });
    assets.textureAsync("images/red.png", [&](graphics::Texture texture, std::string) {
        second = texture;
        ++callbacks;
    });
    assets.textureAsync("images/missing.png", [&](graphics::Texture, std::string message) {
        error = message;
        ++callbacks;
    });
    // clang-format on
    EXPECT_GE(assets.getPendingCount(), 1U);

    ASSERT_TRUE(fixture.frameUntil([&] { return callbacks == 3; }));
    EXPECT_TRUE(first.isValid());
    EXPECT_EQ(first, second);
    EXPECT_NE(error.find("missing.png"), std::string::npos);

    bool cached = false;
    assets.textureAsync("images/red.png", [&](graphics::Texture texture, std::string) { cached = texture == first; });
    EXPECT_FALSE(cached);
    ASSERT_TRUE(fixture.frameUntil([&] { return cached; }));

    std::string decodeError;
    assets.loadAsync("json", "data/broken.json", [&](std::shared_ptr<void>, std::string message) { decodeError = message; });
    ASSERT_TRUE(fixture.frameUntil([&] { return !decodeError.empty(); }));
}

TEST_F(AssetsManagerTest, SharesOneAssetBetweenBackgroundAndDirectLoads) {
    test::EngineFixture fixture(getAssetFiles());
    assets::Manager& assets = fixture.engine().getAssets();

    // A direct load while the same asset decodes in the background caches it first, and the background load hands out that one.
    graphics::Texture background;
    assets.textureAsync("images/red.png", [&background](graphics::Texture texture, std::string) { background = texture; });
    const graphics::Texture direct = assets.texture("images/red.png");
    ASSERT_TRUE(fixture.frameUntil([&] { return background.isValid(); }));
    EXPECT_EQ(background, direct);
    EXPECT_EQ(assets.texture("images/red.png"), direct);
    EXPECT_EQ(assets.getCachedCount(), 1U);
}

TEST_F(AssetsManagerTest, SpreadsUploadsOverFramesWithinTheBudget) {
    test::EngineFixture fixture(getAssetFiles());
    assets::Manager& assets = fixture.engine().getAssets();
    EXPECT_DOUBLE_EQ(assets.getUploadBudget(), 0.004);
    EXPECT_THROW(assets.setUploadBudget(-1.0), std::invalid_argument);

    // Without a budget, every frame finalizes a single decoded asset, so a burst of loads never lands in one frame.
    assets.setUploadBudget(0.0);
    int finished = 0;
    for (const char* path : {"images/red.png", "images/blue.png", "data/config.json"}) {
        assets.loadAsync(assets.getTypeForPath(path), path, [&finished](std::shared_ptr<void> asset, std::string) { finished += asset ? 1 : 0; });
    }
    int most = 0;
    int last = 0;
    // clang-format off
    ASSERT_TRUE(fixture.frameUntil([&] {
        most = std::max(most, finished - last);
        last = finished;
        return finished == 3;
    }));
    // clang-format on
    EXPECT_EQ(most, 1);

    // An app in the background creates no GPU resources, and the uploads wait until it comes back.
    assets.setUploadBudget(1.0);
    fixture.engine().handleEvent({.type = platform::Event::Type::Suspended});
    bool uploaded = false;
    assets.textureAsync("images/red.png", [&uploaded](graphics::Texture texture, std::string) { uploaded = texture.isValid(); }, {.filter = graphics::Texture::Filter::Linear});
    ASSERT_TRUE(fixture.frameUntil([&] { return assets.getUploadCount() == 1; }));
    fixture.frames(3);
    EXPECT_FALSE(uploaded);
    fixture.engine().handleEvent({.type = platform::Event::Type::Resumed});
    fixture.frames(1);
    EXPECT_TRUE(uploaded);
}

TEST_F(AssetsManagerTest, PreloadsAndUnloadsGroups) {
    test::EngineFixture fixture(getAssetFiles());
    assets::Manager& assets = fixture.engine().getAssets();
    assets.defineGroups(assets.json("preload.json"));
    EXPECT_EQ(assets.getGroups(), (std::vector<std::string>{"broken", "empty", "menu"}));

    std::vector<float> progress;
    std::vector<std::string> errors{"unset"};
    int completions = 0;
    // clang-format off
    assets.preload("menu", [&](float fraction) { progress.push_back(fraction); }, [&](std::vector<std::string> failures) {
        errors = failures;
        ++completions;
    });
    // clang-format on
    assets.preload("menu", {}, [&](std::vector<std::string>) { ++completions; });
    EXPECT_FALSE(assets.isGroupLoaded("menu"));
    EXPECT_LT(assets.getGroupProgress("menu"), 1.0F);

    ASSERT_TRUE(fixture.frameUntil([&] { return completions == 2; }));
    EXPECT_TRUE(errors.empty());
    EXPECT_EQ(progress.size(), 3U);
    EXPECT_FLOAT_EQ(progress.back(), 1.0F);
    EXPECT_TRUE(assets.isGroupLoaded("menu"));
    EXPECT_FLOAT_EQ(assets.getGroupProgress("menu"), 1.0F);

    bool again = false;
    assets.preload("menu", [&](float fraction) { again = fraction >= 1.0F; });
    ASSERT_TRUE(fixture.frameUntil([&] { return again; }));

    assets.unloadGroup("menu");
    EXPECT_FALSE(assets.isGroupLoaded("menu"));
    EXPECT_FLOAT_EQ(assets.getGroupProgress("menu"), 0.0F);
    EXPECT_GE(assets.releaseUnused(), 2U);

    std::vector<std::string> brokenErrors;
    assets.preload("broken", {}, [&](std::vector<std::string> failures) { brokenErrors = failures; });
    ASSERT_TRUE(fixture.frameUntil([&] { return brokenErrors.size() == 2; }));

    // A group that loaded completes every later preload with the failures of its load.
    std::vector<std::string> repeatedErrors;
    assets.preload("broken", {}, [&](std::vector<std::string> failures) { repeatedErrors = failures; });
    ASSERT_TRUE(fixture.frameUntil([&] { return !repeatedErrors.empty(); }));
    EXPECT_EQ(repeatedErrors, brokenErrors);

    bool emptyDone = false;
    assets.preload("empty", {}, [&](std::vector<std::string>) { emptyDone = true; });
    ASSERT_TRUE(fixture.frameUntil([&] { return emptyDone; }));

    EXPECT_THROW(assets.preload("missing"), std::invalid_argument);
    EXPECT_THROW(assets.unloadGroup("missing"), std::invalid_argument);
    EXPECT_THROW((void)assets.getGroupProgress("missing"), std::invalid_argument);
    EXPECT_FALSE(assets.isGroupLoaded("missing"));
}

TEST_F(AssetsManagerTest, KeepsAGroupAsItWasWhenItsPreloadFails) {
    test::EngineFixture fixture(getAssetFiles());
    assets::Manager& assets = fixture.engine().getAssets();

    // An entry without an asset type and an entry with invalid options fail the preload before the group gains a listener or starts to load.
    const std::vector<assets::GroupManifest::Entry> untyped{{.path = "images/readme.txt"}};
    const std::vector<assets::GroupManifest::Entry> misconfigured{{.path = "images/blue.png"}, {.path = "images/red.png", .options = {{"filter", "blurry"}}}};
    int staleProgress = 0;
    int staleCompletions = 0;
    for (const std::vector<assets::GroupManifest::Entry>* broken : {&untyped, &misconfigured}) {
        assets.defineGroup("menu", *broken);
        EXPECT_THROW(assets.preload("menu", [&](float) { ++staleProgress; }, [&](std::vector<std::string>) { ++staleCompletions; }), std::invalid_argument);
        EXPECT_FALSE(assets.isGroupLoaded("menu"));

        assets.defineGroup("menu", {{.path = "images/blue.png"}});
        std::vector<float> progress;
        bool completed = false;
        assets.preload("menu", [&](float fraction) { progress.push_back(fraction); }, [&](std::vector<std::string>) { completed = true; });
        ASSERT_TRUE(fixture.frameUntil([&] { return completed; }));
        EXPECT_EQ(progress, std::vector<float>{1.0F});
        assets.unloadGroup("menu");
    }
    fixture.frames(1);
    EXPECT_EQ(staleProgress, 0);
    EXPECT_EQ(staleCompletions, 0);
}

TEST_F(AssetsManagerTest, UnloadingDuringALoadDropsItsResults) {
    test::EngineFixture fixture(getAssetFiles());
    assets::Manager& assets = fixture.engine().getAssets();
    assets.defineGroup("menu", {{.path = "images/red.png"}});

    // A preload that still waits completes with the failure that its group unloaded first.
    std::vector<std::string> failures;
    assets.preload("menu", {}, [&](std::vector<std::string> errors) { failures = errors; });
    assets.unloadGroup("menu");
    ASSERT_TRUE(fixture.frameUntil([&] { return assets.getPendingCount() == 0 && !failures.empty(); }));
    EXPECT_EQ(failures, (std::vector<std::string>{"The asset group \"menu\" was unloaded before it finished loading."}));
    EXPECT_FALSE(assets.isGroupLoaded("menu"));

    // An empty group completes on the next frame, and unloading it before then keeps it unloaded.
    assets.defineGroup("empty", {});
    std::vector<std::string> emptyFailures;
    assets.preload("empty", {}, [&](std::vector<std::string> errors) { emptyFailures = errors; });
    assets.unloadGroup("empty");
    ASSERT_TRUE(fixture.frameUntil([&] { return !emptyFailures.empty(); }));
    fixture.frames(1);
    EXPECT_EQ(emptyFailures, (std::vector<std::string>{"The asset group \"empty\" was unloaded before it finished loading."}));
    EXPECT_FALSE(assets.isGroupLoaded("empty"));

    bool completed = false;
    assets.preload("menu", {}, [&](std::vector<std::string>) { completed = true; });
    assets.cancelAll();
    EXPECT_EQ(assets.getPendingCount(), 0U);

    bool reloaded = false;
    assets.textureAsync("images/red.png", [&](graphics::Texture texture, std::string) { reloaded = texture.isValid(); });
    ASSERT_TRUE(fixture.frameUntil([&] { return reloaded; }));
    EXPECT_FALSE(completed);
    EXPECT_FALSE(assets.isGroupLoaded("menu"));
}

TEST_F(AssetsManagerTest, ReloadsFilesWhosePathsHoldAnyCharacter) {
    test::EngineFixture fixture({{"content/data/a|b.json", R"({"level": 1})"}});
    assets::Manager& assets = fixture.engine().getAssets();

    const std::shared_ptr<void> held = assets.load("json", "data/a|b.json");
    fixture.package().setFile("content/data/a|b.json", test::TestFiles::bytes(R"({"level": 2})"));
    EXPECT_EQ(assets.reload("data/a|b.json"), 1U);
    EXPECT_EQ(assets.json("data/a|b.json").at("level"), 2);
}

TEST_F(AssetsManagerTest, RegistersCustomTypes) {
    test::EngineFixture fixture({{"content/levels/one.level", "3"}, {"content/levels/one.fragile", "3"}});
    assets::Manager& assets = fixture.engine().getAssets();
    // clang-format off
    assets.registerType({
        .name = "level",
        .extensions = {".LEVEL"},
        .normalize = [](const core::Json& options) { return options; },
        .decode = [](assets::Manager::Request& request) -> std::shared_ptr<void> { return std::make_shared<int>(std::stoi(std::string(request.bytes.begin(), request.bytes.end()))); },
        .finalize = [](std::shared_ptr<void> decoded, const assets::Manager::Request&) { return decoded; },
    });
    // clang-format on

    EXPECT_TRUE(assets.hasType("level"));
    EXPECT_EQ(assets.getTypeForPath("levels/one.level"), "level");
    EXPECT_EQ(*std::static_pointer_cast<int>(assets.load("level", "levels/one.level")), 3);
    EXPECT_THROW(assets.registerType({.name = "broken"}), std::invalid_argument);

    // A type whose resources fail to build on the frame thread hands the failure to the load that waits for it, which then no longer counts as pending.
    // clang-format off
    assets.registerType({
        .name = "fragile",
        .extensions = {".fragile"},
        .normalize = [](const core::Json& options) { return options; },
        .decode = [](assets::Manager::Request&) -> std::shared_ptr<void> { return std::make_shared<int>(0); },
        .finalize = [](std::shared_ptr<void>, const assets::Manager::Request&) -> std::shared_ptr<void> { throw std::runtime_error("The fragile asset could not be built."); },
    });
    // clang-format on
    std::string failure;
    assets.loadAsync("fragile", "levels/one.fragile", [&failure](std::shared_ptr<void> asset, std::string error) { failure = asset ? "loaded" : error; });
    ASSERT_TRUE(fixture.frameUntil([&] { return !failure.empty(); }));
    EXPECT_EQ(failure, "The fragile asset could not be built.");
    EXPECT_EQ(assets.getPendingCount(), 0U);

    // Lua loads an asset of a new type once the plugin of the type tells how Lua sees it.
    fixture.runLua("assets = require('haylen.assets')");
    EXPECT_NE(fixture.lua("assets.load('levels/one.level')").find("The asset type \"level\" has no Lua representation."), std::string::npos);
    fixture.engine().getPlugin<plugins::AssetsPlugin>().registerLuaPusher("level", [](lua_State* L, const std::shared_ptr<void>& asset) { lua_pushinteger(L, *std::static_pointer_cast<int>(asset)); });
    EXPECT_EQ(fixture.lua("return assets.load('levels/one.level')"), "3");
}

// Assets that name the same file load it once through the cache before they are finalized, however many of them load at the same time.
TEST_F(AssetsManagerTest, LoadsTheDependenciesOfAnAssetOnceThroughTheCache) {
    test::EngineFixture fixture({{"content/tiles/grass.tile", "7"}, {"content/maps/one.map", "tiles/grass.tile"}, {"content/maps/two.map", "tiles/grass.tile"}, {"content/maps/three.map", "tiles/grass.tile"}, {"content/maps/lost.map", "tiles/missing.tile"}});
    assets::Manager& assets = fixture.engine().getAssets();
    auto decodedTiles = std::make_shared<std::atomic<int>>(0);
    // clang-format off
    assets.registerType({
        .name = "tile",
        .extensions = {".tile"},
        .normalize = [](const core::Json& options) { return options; },
        .decode = [decodedTiles](assets::Manager::Request& request) -> std::shared_ptr<void> {
            ++*decodedTiles;
            return std::make_shared<int>(std::stoi(std::string(request.bytes.begin(), request.bytes.end())));
        },
        .finalize = [](std::shared_ptr<void> decoded, const assets::Manager::Request&) { return decoded; },
    });
    assets.registerType({
        .name = "map",
        .extensions = {".map"},
        .normalize = [](const core::Json& options) { return options; },
        .decode = [](assets::Manager::Request& request) -> std::shared_ptr<void> { return std::make_shared<std::string>(request.bytes.begin(), request.bytes.end()); },
        .finalize = [&assets](std::shared_ptr<void> decoded, const assets::Manager::Request&) -> std::shared_ptr<void> {
            return assets.load("tile", *std::static_pointer_cast<std::string>(decoded));
        },
        .dependencies = [](const std::shared_ptr<void>& decoded, const assets::Manager::Request&) {
            return std::vector<assets::Manager::Dependency>{{.type = "tile", .path = *std::static_pointer_cast<std::string>(decoded), .options = core::Json::object()}};
        },
    });
    // clang-format on

    std::vector<std::shared_ptr<void>> maps;
    std::string failure;
    for (const char* name : {"maps/one.map", "maps/two.map", "maps/three.map"}) {
        assets.loadAsync("map", name, [&maps](std::shared_ptr<void> asset, std::string) { maps.push_back(std::move(asset)); });
    }
    assets.loadAsync("map", "maps/lost.map", [&failure](std::shared_ptr<void> asset, std::string error) { failure = asset ? "loaded" : error; });
    ASSERT_TRUE(fixture.frameUntil([&] { return maps.size() == 3 && !failure.empty(); }));
    EXPECT_EQ(decodedTiles->load(), 1);
    EXPECT_EQ(*std::static_pointer_cast<int>(maps.front()), 7);
    EXPECT_EQ(maps.front(), maps.back());
    EXPECT_NE(failure.find("tiles/missing.tile"), std::string::npos);
    EXPECT_EQ(assets.getPendingCount(), 0U);

    // A synchronous load loads the dependencies first as well, and finds the tile in the cache.
    EXPECT_EQ(assets.load("map", "maps/one.map"), maps.front());
    EXPECT_EQ(decodedTiles->load(), 1);
}

} // namespace haylen::assets
