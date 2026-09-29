#include <gtest/gtest.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include "haylen/assets/Manager.hpp"
#include "haylen/core/Engine.hpp"
#include "haylen/core/Log.hpp"
#include "haylen/io/Package.hpp"
#include "haylen/io/PackageWatcher.hpp"
#include "haylen/plugins/HotReloadPlugin.hpp"
#include "platform/headless/HeadlessHost.hpp"
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

    // Only app.json, source, content and the manifests and Lua modules of plugins belong to the package, so files next to them and the native parts of plugins never count.
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

TEST_F(HotReloadPluginTest, RestartsForScriptsAndReloadsAssets) {
    const test::TemporaryDirectory directory;
    directory.write("hot/app.json", R"({"name": "Hot"})");
    directory.write("hot/source/main.lua", "");
    directory.write("hot/content/tile.png", toText(test::TestFiles::pngImage(2, 2, 0xFFFFFFFFU)));

    // The scans run on the I/O pool, so the frames go on until what a scan found reached the engine.
    std::vector<std::string> log;
    std::mutex logMutex;
    // clang-format off
    const std::uint64_t listener = core::Log::addListener([&](core::Log::Level, std::string_view line) {
        const std::scoped_lock lock(logMutex);
        log.emplace_back(line);
    });
    // clang-format on

    platform::HeadlessHost host(directory.getPath() / "data");
    core::AppConfig config;
    config.hotReload = true;
    core::Engine engine(host, io::Package::openDirectory(directory.getPath() / "hot"), config, std::make_unique<test::TestApplication>(nullptr));
    engine.start();
    EXPECT_TRUE(engine.getPlugin<plugins::HotReloadPlugin>().isWatching());
    const graphics::Texture tile = engine.getAssets().texture("tile.png");

    // clang-format off
    const auto logged = [&](std::string_view text) {
        const std::scoped_lock lock(logMutex);
        return std::ranges::any_of(log, [text](const std::string& line) { return line.find(text) != std::string::npos; });
    };
    const auto runUntil = [&engine](const std::function<bool()>& condition) {
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
        while (!condition() && std::chrono::steady_clock::now() < deadline) {
            engine.frame(0.1);
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        return condition();
    };
    // clang-format on
    EXPECT_TRUE(runUntil([&] { return logged("for changes."); })) << "the first scan takes the snapshot that changes count from";
    directory.write("hot/content/tile.png", toText(test::TestFiles::pngImage(6, 6, 0xFFFFFFFFU)));
    touch(directory.getPath() / "hot/content/tile.png", 5);
    EXPECT_TRUE(runUntil([&] { return tile.getWidth() == 6; }));
    EXPECT_FALSE(engine.isRestartRequested());

    directory.write("hot/content/tile.png", "half written");
    touch(directory.getPath() / "hot/content/tile.png", 10);
    EXPECT_TRUE(runUntil([&] { return logged("tile.png could not be reloaded yet"); }));
    EXPECT_EQ(engine.getError(), nullptr);
    EXPECT_EQ(tile.getWidth(), 6);

    // Files next to the package never count, which the scan that reloads the tile again shows.
    directory.write("hot/README.md", "Not part of the package.");
    directory.write("hot/content/tile.png", toText(test::TestFiles::pngImage(3, 3, 0xFFFFFFFFU)));
    touch(directory.getPath() / "hot/content/tile.png", 15);
    EXPECT_TRUE(runUntil([&] { return tile.getWidth() == 3; }));
    EXPECT_FALSE(engine.isRestartRequested());

    directory.write("hot/source/main.lua", "-- edited");
    touch(directory.getPath() / "hot/source/main.lua", 5);
    EXPECT_TRUE(runUntil([&] { return engine.isRestartRequested(); }));
    core::Log::removeListener(listener);
}

TEST_F(HotReloadPluginTest, RestartsWhenTheLuaOfAPluginChanges) {
    const test::TemporaryDirectory directory;
    directory.write("hot/app.json", R"({"name": "Hot", "plugins": {"ads": {}}})");
    directory.write("hot/source/main.lua", "");
    directory.write("hot/plugins/ads/plugin.json", R"({"id": "ads", "version": "1.0.0"})");
    directory.write("hot/plugins/ads/source/init.lua", "return {}");

    std::atomic<int> scans = 0;
    // clang-format off
    const std::uint64_t listener = core::Log::addListener([&scans](core::Log::Level, std::string_view line) {
        if (line.find("for changes.") != std::string_view::npos) {
            ++scans;
        }
    });
    // clang-format on

    platform::HeadlessHost host(directory.getPath() / "data");
    const std::shared_ptr<io::Package> package = io::Package::openDirectory(directory.getPath() / "hot");
    core::AppConfig config = core::AppConfig::fromPackage(*package);
    config.hotReload = true;
    core::Engine engine(host, package, config, std::make_unique<test::TestApplication>(nullptr));
    engine.start();

    // clang-format off
    const auto runUntil = [&engine](const std::function<bool()>& condition) {
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
        while (!condition() && std::chrono::steady_clock::now() < deadline) {
            engine.frame(0.1);
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        return condition();
    };
    // clang-format on
    EXPECT_TRUE(runUntil([&scans] { return scans > 0; }));

    // The native part of a plugin is no Lua, so only the edited module of the plugin restarts the app.
    directory.write("hot/plugins/ads/android/build.gradle.kts", "plugins {}");
    directory.write("hot/plugins/ads/source/init.lua", "return {edited = true}");
    touch(directory.getPath() / "hot/plugins/ads/source/init.lua", 5);
    EXPECT_TRUE(runUntil([&engine] { return engine.isRestartRequested(); }));
    core::Log::removeListener(listener);
}

TEST_F(HotReloadPluginTest, StaysIdleForShippedApps) {
    test::EngineFixture fixture;
    EXPECT_FALSE(fixture.engine().getPlugin<plugins::HotReloadPlugin>().isWatching());
    EXPECT_FALSE(fixture.engine().isRestartRequested());
    fixture.engine().requestRestart();
    EXPECT_TRUE(fixture.engine().isRestartRequested());
}

} // namespace haylen::debug
