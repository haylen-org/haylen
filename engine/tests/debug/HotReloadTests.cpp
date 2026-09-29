#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include "haylen/assets/Manager.hpp"
#include "haylen/core/Engine.hpp"
#include "haylen/io/Package.hpp"
#include "haylen/io/PackageWatcher.hpp"
#include "haylen/plugins/HotReloadPlugin.hpp"
#include "platform/headless/HeadlessHost.hpp"
#include "support/EngineFixture.hpp"
#include "support/TemporaryDirectory.hpp"
#include "support/TestApplication.hpp"

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
    io::PackageWatcher watcher(directory.getPath());
    EXPECT_TRUE(watcher.scan().empty());

    // Only app.json, source and content belong to the package, so files next to them never count.
    directory.write("content/b.json", "[]");
    directory.write("README.md", "# Notes");
    directory.write("platform/android/build/other.txt", "");
    touch(directory.getPath() / "source/main.lua", 5);
    touch(directory.getPath() / "app.json", 5);
    std::filesystem::remove(directory.getPath() / "content/a.json");
    EXPECT_EQ(watcher.scan(), (std::vector<std::string>{"app.json", "content/a.json", "content/b.json", "source/main.lua"}));
    EXPECT_TRUE(watcher.scan().empty());
}

TEST_F(AssetReloadTest, UpdatesTexturesInPlaceAndDropsOtherAssets) {
    test::EngineFixture fixture({{"content/hero.png", toText(test::pngImage(4, 4, 0xFF0000FFU))}, {"content/data.json", R"({"level": 1})"}});
    assets::Manager& assets = fixture.engine().getAssets();
    const graphics::Texture hero = assets.texture("hero.png");
    EXPECT_EQ(assets.json("data.json").at("level"), 1);
    EXPECT_EQ(hero.getWidth(), 4);

    fixture.package().setFile("content/hero.png", test::pngImage(8, 2, 0x00FF00FFU));
    fixture.package().setFile("content/data.json", test::bytes(R"({"level": 2})"));
    EXPECT_EQ(assets.reload("hero.png"), 1U);
    EXPECT_EQ(hero.getWidth(), 8);
    EXPECT_EQ(hero.getHeight(), 2);
    EXPECT_EQ(assets.reload("data.json"), 0U);
    EXPECT_EQ(assets.json("data.json").at("level"), 2);
    EXPECT_EQ(assets.reload("missing.png"), 0U);

    fixture.package().setFile("content/hero.png", test::bytes("broken"));
    EXPECT_THROW((void)assets.reload("hero.png"), std::runtime_error);
}

TEST_F(HotReloadPluginTest, RestartsForScriptsAndReloadsAssets) {
    const test::TemporaryDirectory directory;
    directory.write("hot/app.json", R"({"name": "Hot"})");
    directory.write("hot/source/main.lua", "");
    directory.write("hot/content/tile.png", toText(test::pngImage(2, 2, 0xFFFFFFFFU)));

    platform::HeadlessHost host(directory.getPath() / "data");
    core::AppConfig config;
    config.hotReload = true;
    core::Engine engine(host, io::Package::openDirectory(directory.getPath() / "hot"), config, std::make_unique<test::TestApplication>(nullptr));
    engine.start();
    EXPECT_TRUE(engine.getPlugin<plugins::HotReloadPlugin>().isWatching());
    const graphics::Texture tile = engine.getAssets().texture("tile.png");

    // clang-format off
    const auto run = [&engine](int frames) {
        for (int frame = 0; frame < frames; ++frame) {
            engine.frame(0.1);
        }
    };
    // clang-format on
    directory.write("hot/content/tile.png", toText(test::pngImage(6, 6, 0xFFFFFFFFU)));
    touch(directory.getPath() / "hot/content/tile.png", 5);
    run(6);
    EXPECT_EQ(tile.getWidth(), 6);
    EXPECT_FALSE(engine.isRestartRequested());

    directory.write("hot/content/tile.png", "half written");
    touch(directory.getPath() / "hot/content/tile.png", 10);
    run(6);
    EXPECT_FALSE(engine.getError() != nullptr);
    EXPECT_EQ(tile.getWidth(), 6);

    directory.write("hot/README.md", "Not part of the package.");
    run(6);
    EXPECT_FALSE(engine.isRestartRequested());

    directory.write("hot/source/main.lua", "-- edited");
    touch(directory.getPath() / "hot/source/main.lua", 5);
    run(6);
    EXPECT_TRUE(engine.isRestartRequested());
}

TEST_F(HotReloadPluginTest, StaysIdleForShippedApps) {
    test::EngineFixture fixture;
    EXPECT_FALSE(fixture.engine().getPlugin<plugins::HotReloadPlugin>().isWatching());
    EXPECT_FALSE(fixture.engine().isRestartRequested());
    fixture.engine().requestRestart();
    EXPECT_TRUE(fixture.engine().isRestartRequested());
}

} // namespace haylen::debug
