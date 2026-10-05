#include <gtest/gtest.h>

#include <lua.hpp>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include "haylen/assets/Manager.hpp"
#include "haylen/core/Engine.hpp"
#include "haylen/core/Log.hpp"
#include "haylen/io/Package.hpp"
#include "haylen/io/PackageWatcher.hpp"
#include "haylen/lua/Application.hpp"
#include "platform/DevelopmentSession.hpp"
#include "platform/headless/HeadlessHost.hpp"
#include "plugins/HotReloadPlugin.hpp"
#include "support/EngineFixture.hpp"
#include "support/TemporaryDirectory.hpp"
#include "support/TestApplication.hpp"
#include "support/TestFiles.hpp"

namespace haylen::debug {

// Edits package files the way an editor does while a test watches them.
class PackageEditTest : public ::testing::Test {
  protected:
    [[nodiscard]] static std::string toText(const std::vector<std::uint8_t>& bytes) {
        return {reinterpret_cast<const char*>(bytes.data()), bytes.size()};
    }

    // File times can be coarser than a test runs, so changes move the time forward explicitly.
    static void touch(const std::filesystem::path& file, int seconds) {
        std::filesystem::last_write_time(file, std::filesystem::last_write_time(file) + std::chrono::seconds(seconds));
    }
};

class PackageWatcherTest : public PackageEditTest {};

class AssetReloadTest : public PackageEditTest {};

class HotReloadPluginTest : public PackageEditTest {};

TEST_F(PackageWatcherTest, ReportsAddedChangedAndRemovedFiles) {
    const test::TemporaryDirectory directory;
    directory.write("app.json", "{}");
    directory.write("source/main.lua", "print(1)");
    directory.write("content/a.json", "{}");
    directory.write("platform/android/build/output.txt", "");
    directory.write("plugins/ads/plugin.json", "{}");
    directory.write("plugins/ads/source/init.lua", "return {}");
    directory.write("plugins/ads/android/build.gradle.kts", "");
    io::PackageWatcher watcher(directory.getPath());
    EXPECT_TRUE(watcher.scan().empty());

    // Only `app.json`, `source`, `content` and the manifests and Lua modules of plugins belong to the package, so files next to them and the native parts of plugins never count.
    directory.write("content/b.json", "[]");
    directory.write("README.md", "# Notes");
    directory.write("platform/android/build/other.txt", "");
    directory.write("plugins/README.md", "# Plugins");
    directory.write("plugins/ads/android/src/main/AndroidManifest.xml", "");
    directory.write("plugins/ads/source/banner.lua", "return {}");
    touch(directory.getPath() / "source/main.lua", 5);
    touch(directory.getPath() / "app.json", 5);
    touch(directory.getPath() / "plugins/ads/plugin.json", 5);
    std::filesystem::remove(directory.getPath() / "content/a.json");
    EXPECT_EQ(watcher.scan(), (std::vector<std::string>{"app.json", "content/a.json", "content/b.json", "plugins/ads/plugin.json", "plugins/ads/source/banner.lua", "source/main.lua"}));
    EXPECT_TRUE(watcher.scan().empty());
}

TEST_F(PackageWatcherTest, IgnoresHiddenBackupAndNonLuaSourceFiles) {
    const test::TemporaryDirectory directory;
    directory.write("app.json", "{}");
    directory.write("source/main.lua", "");
    directory.write("plugins/ads/plugin.json", "{}");
    io::PackageWatcher watcher(directory.getPath());

    // Editors write swap, backup and lock files next to the file they save, which must never restart the app.
    directory.write("source/.level.lua.swp", "swap");
    directory.write("source/level.lua~", "backup");
    directory.write("source/#level.lua#", "autosave");
    directory.write("source/level.lua.orig", "merge");
    directory.write("source/notes.txt", "notes");
    directory.write("source/.cache/level.lua", "cache");
    directory.write("content/.cache", "cache");
    directory.write("content/hero.png.tmp", "half");
    directory.write("plugins/ads/source/notes.md", "notes");
    directory.write("source/level.lua", "return {}");
    directory.write("content/hero.png", "png");
    directory.write("plugins/ads/source/banner.lua", "return {}");
    EXPECT_EQ(watcher.scan(), (std::vector<std::string>{"content/hero.png", "plugins/ads/source/banner.lua", "source/level.lua"}));

    EXPECT_TRUE(io::PackageWatcher::isWatched("app.json"));
    EXPECT_TRUE(io::PackageWatcher::isWatched("plugins/ads/plugin.json"));
    EXPECT_FALSE(io::PackageWatcher::isWatched("plugins/ads/android/build.gradle.kts"));
    EXPECT_FALSE(io::PackageWatcher::isWatched("platform/web/index.html"));
    EXPECT_FALSE(io::PackageWatcher::isWatched("source/.level.lua.swp"));
}

TEST_F(PackageWatcherTest, WaitsForFilesToSettle) {
    const test::TemporaryDirectory directory;
    directory.write("content/level.json", "{}");
    io::PackageWatcher watcher(directory.getPath());

    // A file that grows while the scan looks at it is still being written, so the scan leaves it for later.
    std::atomic<bool> writing = true;
    // clang-format off
    std::thread writer([&directory, &writing] {
        std::string content = "{";
        while (writing) {
            content.push_back(' ');
            directory.write("content/level.json", content + "}");
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    });
    // clang-format on
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    const std::vector<std::string> busy = watcher.scan();
    writing = false;
    writer.join();
    EXPECT_TRUE(busy.empty());
    EXPECT_EQ(watcher.scan(), (std::vector<std::string>{"content/level.json"}));
    EXPECT_TRUE(watcher.scan().empty());
}

TEST_F(AssetReloadTest, UpdatesTexturesInPlaceAndDropsOtherAssets) {
    test::EngineFixture fixture({{"content/hero.png", toText(test::TestFiles::pngImage(4, 4, 0xFF0000FFU))}, {"content/data.json", R"({"level": 1})"}});
    assets::Manager& assets = fixture.engine().getAssets();
    const graphics::Texture hero = assets.texture("hero.png");
    EXPECT_EQ(assets.json("data.json").at("level"), 1);
    EXPECT_EQ(hero.getWidth(), 4);

    fixture.package().setFile("content/hero.png", test::TestFiles::pngImage(8, 2, 0x00FF00FFU));
    fixture.package().setFile("content/data.json", test::TestFiles::bytes(R"({"level": 2})"));
    EXPECT_EQ(assets.reload("hero.png"), 1U);
    EXPECT_EQ(hero.getWidth(), 8);
    EXPECT_EQ(hero.getHeight(), 2);
    EXPECT_EQ(assets.reload("data.json"), 0U);
    EXPECT_EQ(assets.json("data.json").at("level"), 2);
    EXPECT_EQ(assets.reload("missing.png"), 0U);

    fixture.package().setFile("content/hero.png", test::TestFiles::bytes("broken"));
    EXPECT_THROW((void)assets.reload("hero.png"), std::runtime_error);
}

TEST_F(AssetReloadTest, UpdatesJsonInPlaceAndAnnouncesStaleAssets) {
    test::EngineFixture fixture({{"content/data.json", R"({"level": 1})"}, {"content/icon.svg", R"(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 12"><path d="M0 0h24v12z"/></svg>)"}});
    // clang-format off
    fixture.runLua(R"(
        heard = {}
        local events = require('haylen.events')
        events.on('assetReloaded', function(event) heard[#heard + 1] = 'reloaded ' .. event.type .. ' ' .. event.path end)
        events.on('assetChanged', function(event) heard[#heard + 1] = 'changed ' .. event.type .. ' ' .. event.path end)
        icon = require('haylen.assets').vectorImage('icon.svg')
    )");
    // clang-format on
    assets::Manager& assets = fixture.engine().getAssets();
    const std::shared_ptr<void> held = assets.load("json", "data.json");

    fixture.package().setFile("content/data.json", test::TestFiles::bytes(R"({"level": 2})"));
    fixture.package().setFile("content/icon.svg", test::TestFiles::bytes(R"(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 48 12"><path d="M0 0h48v12z"/></svg>)"));
    EXPECT_EQ(assets.reload("data.json"), 1U);
    EXPECT_EQ(assets.reload("icon.svg"), 1U);
    EXPECT_EQ(assets.json("data.json").at("level"), 2);
    fixture.frames(1);
    EXPECT_EQ(fixture.lua("return table.concat(heard, ', ')"), "reloaded json data.json, changed vectorImage icon.svg");
}

TEST_F(AssetReloadTest, RebuildsCatalogsAndThemesFromChangedFiles) {
    // clang-format off
    test::EngineFixture fixture({
        {"content/locale/en.json", R"({"greeting": "Hello"})"},
        {"content/themes/wood.json", R"({"name": "wood", "colors": {"accent": "#FF102030"}})"},
        {"source/main.lua", R"(
            localization = require('haylen.localization')
            ui = require('haylen.ui')
            localization.loadFolder('locale')
            ui.setTheme(ui.loadTheme('themes/wood.json', 'light'))
        )"},
    }, nullptr, {.development = true});
    // clang-format on
    EXPECT_EQ(fixture.lua("return localization.text('greeting')"), "Hello");

    fixture.package().setFile("content/locale/en.json", test::TestFiles::bytes(R"({"greeting": "Hi again"})"));
    fixture.package().setFile("content/themes/wood.json", test::TestFiles::bytes(R"({"name": "wood", "colors": {"accent": "#FF405060"}})"));
    const std::vector<std::string> changed{"content/locale/en.json", "content/themes/wood.json"};
    fixture.host().getDevelopmentSession()->addChanges(changed);
    fixture.frames(1);
    EXPECT_EQ(fixture.lua("return localization.text('greeting')"), "Hi again");
    EXPECT_EQ(fixture.lua("return ui.theme() .. ' ' .. ui.themeColor('accent'):toHex()"), "wood #FF405060");
    EXPECT_FALSE(fixture.engine().isRestartRequested());
}

TEST_F(HotReloadPluginTest, RestartsForChangedFonts) {
    test::EngineFixture fixture({}, nullptr, {.development = true});
    const std::vector<std::string> changed{"content/fonts/title.ttf"};
    fixture.host().getDevelopmentSession()->addChanges(changed);
    fixture.frames(1);
    EXPECT_TRUE(fixture.engine().isRestartRequested());
}

// Scans run on the I/O pool, so a watched engine runs frames until what a scan found reached it.
class WatchedEngine final {
  public:
    explicit WatchedEngine(const std::filesystem::path& folder, std::unique_ptr<core::Application> application = std::make_unique<test::TestApplication>(nullptr)) : host(folder.parent_path() / "data") {
        host.enableDevelopment(folder);
        const std::shared_ptr<io::Package> package = io::Package::openDirectory(folder);
        core::AppConfig config = core::AppConfig::fromPackage(*package);
        application->configure(config);
        engine = std::make_unique<core::Engine>(host, package, std::move(config), std::move(application));
        engine->start();
    }

    [[nodiscard]] core::Engine& get() noexcept {
        return *engine;
    }

    bool runUntil(const std::function<bool()>& condition) {
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
        while (!condition() && std::chrono::steady_clock::now() < deadline) {
            engine->frame(0.1);
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        return condition();
    }

  private:
    platform::HeadlessHost host;
    std::unique_ptr<core::Engine> engine;
};

TEST_F(HotReloadPluginTest, RestartsForScriptsAndReloadsAssets) {
    const test::TemporaryDirectory directory;
    directory.write("hot/app.json", R"({"name": "Hot"})");
    directory.write("hot/source/main.lua", "");
    directory.write("hot/content/tile.png", toText(test::TestFiles::pngImage(2, 2, 0xFFFFFFFFU)));

    std::vector<std::string> log;
    std::mutex logMutex;
    // clang-format off
    const std::uint64_t listener = core::Log::addListener([&](core::Log::Level, std::string_view line) {
        const std::scoped_lock lock(logMutex);
        log.emplace_back(line);
    });
    const auto logged = [&](std::string_view text) {
        const std::scoped_lock lock(logMutex);
        return std::ranges::any_of(log, [text](const std::string& line) { return line.find(text) != std::string::npos; });
    };
    // clang-format on

    WatchedEngine watched(directory.getPath() / "hot");
    core::Engine& engine = watched.get();
    EXPECT_TRUE(engine.getPlugin<plugins::HotReloadPlugin>().isActive());
    const graphics::Texture tile = engine.getAssets().texture("tile.png");
    directory.write("hot/content/tile.png", toText(test::TestFiles::pngImage(6, 6, 0xFFFFFFFFU)));
    touch(directory.getPath() / "hot/content/tile.png", 5);
    EXPECT_TRUE(watched.runUntil([&] { return tile.getWidth() == 6; }));
    EXPECT_FALSE(engine.isRestartRequested());

    directory.write("hot/content/tile.png", "half written");
    touch(directory.getPath() / "hot/content/tile.png", 10);
    EXPECT_TRUE(watched.runUntil([&] { return logged("tile.png\" could not be reloaded yet"); }));
    EXPECT_EQ(engine.getError(), nullptr);
    EXPECT_EQ(tile.getWidth(), 6);

    // Files next to the package never count, which the scan that reloads the tile again shows.
    directory.write("hot/README.md", "Not part of the package.");
    directory.write("hot/content/tile.png", toText(test::TestFiles::pngImage(3, 3, 0xFFFFFFFFU)));
    touch(directory.getPath() / "hot/content/tile.png", 15);
    EXPECT_TRUE(watched.runUntil([&] { return tile.getWidth() == 3; }));
    EXPECT_FALSE(engine.isRestartRequested());

    directory.write("hot/source/main.lua", "-- edited");
    touch(directory.getPath() / "hot/source/main.lua", 5);
    EXPECT_TRUE(watched.runUntil([&] { return engine.isRestartRequested(); }));
    core::Log::removeListener(listener);
}

TEST_F(HotReloadPluginTest, ReloadsTheLuaOfAPluginInPlace) {
    const test::TemporaryDirectory directory;
    directory.write("hot/app.json", R"({"name": "Hot", "plugins": {"ads": {}}})");
    directory.write("hot/source/main.lua", "ads = require('ads')");
    directory.write("hot/plugins/ads/plugin.json", R"({"id": "ads", "version": "1.0.0"})");
    directory.write("hot/plugins/ads/source/init.lua", "return {show = function() return 'v1' end}");
    WatchedEngine watched(directory.getPath() / "hot", std::make_unique<lua::Application>());
    lua_State* L = watched.get().getLuaState();
    // clang-format off
    const auto shows = [L] {
        lua_getglobal(L, "ads");
        lua_getfield(L, -1, "show");
        lua_call(L, 0, 1);
        std::string result = lua_tostring(L, -1);
        lua_pop(L, 2);
        return result;
    };
    // clang-format on

    // The native part of a plugin is no Lua, so only the edited module of the plugin changes, in place.
    directory.write("hot/plugins/ads/android/build.gradle.kts", "plugins {}");
    directory.write("hot/plugins/ads/source/init.lua", "return {show = function() return 'v2' end}");
    touch(directory.getPath() / "hot/plugins/ads/source/init.lua", 5);
    EXPECT_TRUE(watched.runUntil([&shows] { return shows() == "v2"; }));
    EXPECT_FALSE(watched.get().isRestartRequested());
}

TEST_F(HotReloadPluginTest, IgnoresTheFilesOfEditors) {
    const test::TemporaryDirectory directory;
    directory.write("hot/app.json", R"({"name": "Hot"})");
    directory.write("hot/source/main.lua", "");
    directory.write("hot/content/tile.png", toText(test::TestFiles::pngImage(2, 2, 0xFFFFFFFFU)));
    WatchedEngine watched(directory.getPath() / "hot");
    const graphics::Texture tile = watched.get().getAssets().texture("tile.png");

    // The tile that reloads shows that a scan saw the files the editor wrote before it.
    directory.write("hot/source/.main.lua.swp", "swap");
    directory.write("hot/source/main.lua~", "backup");
    directory.write("hot/source/notes.txt", "notes");
    directory.write("hot/content/tile.png", toText(test::TestFiles::pngImage(5, 5, 0xFFFFFFFFU)));
    touch(directory.getPath() / "hot/content/tile.png", 5);
    EXPECT_TRUE(watched.runUntil([&tile] { return tile.getWidth() == 5; }));
    EXPECT_FALSE(watched.get().isRestartRequested());
}

TEST_F(HotReloadPluginTest, StaysIdleForShippedApps) {
    test::EngineFixture fixture;
    EXPECT_FALSE(fixture.engine().getPlugin<plugins::HotReloadPlugin>().isActive());
    EXPECT_EQ(fixture.host().getDevelopmentSession(), nullptr);
    EXPECT_FALSE(fixture.engine().isRestartRequested());
    fixture.engine().requestRestart();
    EXPECT_TRUE(fixture.engine().isRestartRequested());
}

TEST_F(HotReloadPluginTest, KeepsChangesThatArriveDuringARestart) {
    test::EngineFixture fixture({}, nullptr, {.development = true});
    platform::DevelopmentSession& session = *fixture.host().getDevelopmentSession();

    // The session belongs to the host, so a change queued while the engine is replaced reaches the next engine.
    fixture.engine().requestRestart();
    const std::vector<std::string> changed{"app.json"};
    session.addChanges(changed);
    fixture.restart();
    EXPECT_FALSE(fixture.engine().isRestartRequested());
    fixture.frames(1);
    EXPECT_TRUE(fixture.engine().isRestartRequested());
    EXPECT_TRUE(session.takeChanges().empty());
}

TEST_F(HotReloadPluginTest, RestartsFromAFailedAppWhenAppJsonChanges) {
    const test::TemporaryDirectory directory;
    directory.write("broken/app.json", R"({"name": "Broken", "window": {"widht": 100}})");
    directory.write("broken/source/main.lua", "");

    // The runtime plays an app whose configuration fails with defaults and an application that shows the reason, and the app still restarts once the file is fixed.
    platform::HeadlessHost host(directory.getPath() / "data");
    host.enableDevelopment(directory.getPath() / "broken");
    // clang-format off
    core::Engine engine(host, io::Package::openDirectory(directory.getPath() / "broken"), core::AppConfig{}, std::make_unique<test::TestApplication>([](core::Engine&) {
        throw std::runtime_error("The app could not be loaded. Unknown key \"widht\".");
    }));
    // clang-format on
    engine.start();
    ASSERT_NE(engine.getError(), nullptr);
    EXPECT_TRUE(engine.getPlugin<plugins::HotReloadPlugin>().isActive());

    directory.write("broken/app.json", R"({"name": "Broken", "window": {"width": 100}})");
    touch(directory.getPath() / "broken/app.json", 5);
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    while (!engine.isRestartRequested() && std::chrono::steady_clock::now() < deadline) {
        engine.frame(0.1);
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    EXPECT_TRUE(engine.isRestartRequested());
}

} // namespace haylen::debug
