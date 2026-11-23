#include <gtest/gtest.h>
#include <imgui.h>
#include <imgui_internal.h>

#include <lua.hpp>

#include <algorithm>
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

#include "debug/PackageEditTest.hpp"
#include "haylen/assets/Manager.hpp"
#include "haylen/core/Connection.hpp"
#include "haylen/core/Engine.hpp"
#include "haylen/core/Json.hpp"
#include "haylen/core/Log.hpp"
#include "haylen/io/Package.hpp"
#include "haylen/lua/Application.hpp"
#include "haylen/lua/ScriptPlugin.hpp"
#include "haylen/math/Vec2.hpp"
#include "haylen/platform/DevelopmentSession.hpp"
#include "haylen/platform/Event.hpp"
#include "haylen/plugins/HotReloadPlugin.hpp"
#include "haylen/plugins/UiPlugin.hpp"
#include "haylen/text/Font.hpp"
#include "platform/headless/HeadlessHost.hpp"
#include "support/EngineFixture.hpp"
#include "support/FontCollection.hpp"
#include "support/TemporaryDirectory.hpp"
#include "support/TestApplication.hpp"
#include "support/TestFiles.hpp"

namespace haylen::debug {

class HotReloadPluginTest : public PackageEditTest {
  protected:
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
};

// A changed font file and a changed sound file change in place: the font asset takes the new file and lays its text out again, the UI font read from the file reaches ImGui before its next frame, and the sound plays the new samples from its next voice on.
TEST_F(HotReloadPluginTest, ReloadsFontsAndSoundsInPlace) {
    test::EngineFixture fixture({{"source/main.lua", ""}}, nullptr, {.development = true});
    fixture.package().setFile("content/fonts/title.ttf", test::FontCollection::read("lilita-one-regular.ttf"));
    fixture.package().setFile("content/sounds/beep.wav", test::TestFiles::wav(22050, 1, std::vector<float>(2205, 0.5F)));
    fixture.runLua("require('haylen.ui').addFont('title', 'fonts/title.ttf') beep = require('haylen.assets').load('sounds/beep.wav')");
    fixture.frames(1);
    core::Engine& engine = fixture.engine();
    const std::shared_ptr<text::Font> font = engine.getAssets().font("fonts/title.ttf");
    const math::Vec2 before = font->measure("Hello", {.size = 32.0F});
    const int uiFontSize = engine.getPlugin<plugins::UiPlugin>().getBackend().getFont("title")->Sources[0]->FontDataSize;
    core::Json report;
    const core::Connection connection = engine.getPlugin<plugins::HotReloadPlugin>().reloaded.connect([&report](const plugins::HotReloadPlugin::Report& value) { report = value.toJson(); });

    fixture.package().setFile("content/fonts/title.ttf", test::FontCollection::read("crimson-text-regular.ttf"));
    fixture.package().setFile("content/sounds/beep.wav", test::TestFiles::wav(22050, 1, std::vector<float>(22050, 0.5F)));
    const std::vector<std::string> changed{"content/fonts/title.ttf", "content/sounds/beep.wav"};
    fixture.developmentSession().addChanges(changed);
    fixture.frames(2);

    EXPECT_FALSE(engine.isRestartRequested());
    ASSERT_TRUE(report.contains("files"));
    for (const core::Json& file : report.at("files")) {
        EXPECT_EQ(file.at("change"), "inPlace") << file.dump();
    }

    EXPECT_EQ(font->getRevision(), 1U);
    EXPECT_EQ(engine.getAssets().font("fonts/title.ttf"), font);
    EXPECT_NE(font->measure("Hello", {.size = 32.0F}).x, before.x);
    EXPECT_NE(engine.getPlugin<plugins::UiPlugin>().getBackend().getFont("title")->Sources[0]->FontDataSize, uiFontSize);
    EXPECT_EQ(fixture.lua("return string.format('%.1f', beep.duration)"), "1.0");
}

TEST_F(HotReloadPluginTest, RestartsForScriptsAndReloadsAssets) {
    const test::TemporaryDirectory directory;
    directory.write("hot/haylen.json", R"({"name": "Hot"})");
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
    directory.write("hot/haylen.json", R"({"name": "Hot", "plugins": {"ads": {}}})");
    directory.write("hot/source/main.lua", "ads = require('ads')");
    directory.write("hot/plugins/ads/plugin.json", R"({"id": "ads", "version": "1.0.0", "implementations": {"ios": "lua"}})");
    directory.write("hot/plugins/ads/source/init.lua", "return {show = function() return 'v1' end}");
    WatchedEngine watched(directory.getPath() / "hot", std::make_unique<lua::Application>());
    lua_State* L = watched.get().getPlugin<lua::ScriptPlugin>().getState();
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
    directory.write("hot/haylen.json", R"({"name": "Hot"})");
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

TEST_F(HotReloadPluginTest, ScansWhileTheAppIsHalted) {
    const test::TemporaryDirectory directory;
    directory.write("hot/haylen.json", R"({"name": "Hot", "lifecycle": {"pauseOnFocusLoss": true}})");
    directory.write("hot/source/main.lua", "");
    directory.write("hot/content/tile.png", toText(test::TestFiles::pngImage(2, 2, 0xFFFFFFFFU)));
    WatchedEngine watched(directory.getPath() / "hot");
    core::Engine& engine = watched.get();
    const graphics::Texture tile = engine.getAssets().texture("tile.png");

    // The editor takes the focus, which halts an app that pauses on focus loss, and the files it saves still reach the app.
    engine.handleEvent({.type = platform::Event::Type::FocusLost});
    ASSERT_TRUE(engine.isHalted());
    directory.write("hot/content/tile.png", toText(test::TestFiles::pngImage(4, 4, 0xFFFFFFFFU)));
    touch(directory.getPath() / "hot/content/tile.png", 5);
    EXPECT_TRUE(watched.runUntil([&tile] { return tile.getWidth() == 4; }));
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
    platform::DevelopmentSession& session = fixture.developmentSession();

    // The session belongs to the host, so a change queued while the engine is replaced reaches the next engine.
    fixture.engine().requestRestart();
    const std::vector<std::string> changed{"haylen.json"};
    session.addChanges(changed);
    fixture.restart();
    EXPECT_FALSE(fixture.engine().isRestartRequested());
    fixture.frames(1);
    EXPECT_TRUE(fixture.engine().isRestartRequested());
    EXPECT_TRUE(session.takeChanges().empty());
}

TEST_F(HotReloadPluginTest, RestartsFromAFailedAppWhenHaylenJsonChanges) {
    const test::TemporaryDirectory directory;
    directory.write("broken/haylen.json", R"({"name": "Broken", "window": {"widht": 100}})");
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

    directory.write("broken/haylen.json", R"({"name": "Broken", "window": {"width": 100}})");
    touch(directory.getPath() / "broken/haylen.json", 5);
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    while (!engine.isRestartRequested() && std::chrono::steady_clock::now() < deadline) {
        engine.frame(0.1);
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    EXPECT_TRUE(engine.isRestartRequested());
}

} // namespace haylen::debug
