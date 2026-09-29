#include <gtest/gtest.h>

#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include "haylen/2d/graphics/Camera.hpp"
#include "haylen/2d/graphics/Renderer.hpp"
#include "haylen/assets/Manager.hpp"
#include "haylen/core/AppConfig.hpp"
#include "haylen/core/EventBus.hpp"
#include "haylen/core/FrameClock.hpp"
#include "haylen/core/JobSystem.hpp"
#include "haylen/core/LifecycleEvent.hpp"
#include "haylen/core/SceneManager.hpp"
#include "haylen/core/ScopedConnection.hpp"
#include "haylen/core/TimerScheduler.hpp"
#include "haylen/graphics/Viewport.hpp"
#include "haylen/input/ActionMap.hpp"
#include "haylen/input/Input.hpp"
#include "haylen/input/VirtualInput.hpp"
#include "haylen/io/MemoryPackage.hpp"
#include "haylen/lua/Error.hpp"
#include "haylen/platform/Event.hpp"
#include "haylen/plugins/Plugin.hpp"
#include "support/DrawingScene.hpp"
#include "support/EngineFixture.hpp"
#include "support/RecordingScene.hpp"
#include "support/TemporaryDirectory.hpp"
#include "support/TestApplication.hpp"

namespace haylen::core {

namespace {

// Fails at the end of every frame, like a plugin with a bug in its bookkeeping.
class FailingEndPlugin final : public plugins::Plugin {
  public:
    [[nodiscard]] std::string_view getName() const noexcept override {
        return "failing";
    }
    void endFrame(core::Engine&) override {
        throw std::runtime_error("end of frame failed");
    }
};

class CountingPlugin final : public plugins::Plugin {
  public:
    [[nodiscard]] std::string_view getName() const noexcept override {
        return "counting";
    }
    void start(core::Engine&) override {
        ++started;
    }
    void stop(core::Engine&) override {
        ++stopped;
    }
    void event(core::Engine&, const platform::Event&) override {
        ++events;
    }
    void beginFrame(core::Engine&, float) override {
        ++frames;
    }
    void fixedUpdate(core::Engine&, float) override {
        ++fixedSteps;
    }
    void update(core::Engine&, float) override {
        ++updates;
    }
    void render(core::Engine&) override {
        ++renders;
    }
    void renderUi(core::Engine&) override {
        ++uiRenders;
    }
    void endFrame(core::Engine&) override {
        ++endedFrames;
    }

    int started = 0;
    int stopped = 0;
    int events = 0;
    int frames = 0;
    int fixedSteps = 0;
    int updates = 0;
    int renders = 0;
    int uiRenders = 0;
    int endedFrames = 0;
};

} // namespace

TEST(AppConfigTest, ReadsEveryField) {
    const core::AppConfig config = core::AppConfig::fromJson(core::Json::parse(R"({
        "name": "Tiny Island",
        "identifier": "dev.haylen.tinyisland",
        "version": "2.0.0",
        "window": {"title": "Island", "width": 800, "height": 600, "fullscreen": true, "highDpi": false, "resizable": false, "vsync": false, "sampleCount": 4},
        "design": {"width": 1280, "height": 720, "scaling": "fit"},
        "orientation": "portrait",
        "fixedRate": 30,
        "maxFrameTime": 0.1,
        "clearColor": "#102030",
        "splash": {"logo": "ui/logo.png", "background": "#405060"},
        "native": {"steam_api": {"files": {"macos": "platform/apple/libsteam_api.dylib"}}}
    })"));

    EXPECT_EQ(config.name, "Tiny Island");
    EXPECT_EQ(config.identifier, "dev.haylen.tinyisland");
    EXPECT_EQ(config.version, "2.0.0");
    EXPECT_EQ(config.window.title, "Island");
    EXPECT_EQ(config.window.width, 800);
    EXPECT_TRUE(config.window.fullscreen);
    EXPECT_FALSE(config.window.highDpi);
    EXPECT_FALSE(config.window.resizable);
    EXPECT_FALSE(config.window.vsync);
    EXPECT_EQ(config.window.sampleCount, 4);
    EXPECT_EQ(config.designSize, math::Vec2(1280.0F, 720.0F));
    EXPECT_EQ(config.scaling, graphics::Viewport::ScalingPolicy::Fit);
    EXPECT_EQ(config.orientation, platform::Orientation::Portrait);
    EXPECT_EQ(config.fixedRate, 30.0);
    EXPECT_EQ(config.clearColor, math::Color::fromHex(0x102030FFU));
    EXPECT_EQ(config.splash.logo, "ui/logo.png");
    EXPECT_EQ(config.splash.background, math::Color::fromHex(0x405060FFU));
    EXPECT_EQ(core::AppConfig::fromJson(core::Json{{"clearColor", "#102030"}}).splash.background, math::Color::fromHex(0x102030FFU));
    EXPECT_EQ(config.native.at("steam_api").at("files").at("macos"), "platform/apple/libsteam_api.dylib");
    EXPECT_THROW((void)core::AppConfig::fromJson(core::Json{{"native", core::Json::array()}}), std::invalid_argument);

    const core::AppConfig roundTrip = core::AppConfig::fromJson(config.toJson());
    EXPECT_EQ(roundTrip.toJson(), config.toJson());
    for (const char* scaling : {"fit", "fill", "stretch", "expand", "pixelPerfect"}) {
        EXPECT_EQ(core::AppConfig::fromJson(core::Json{{"design", {{"scaling", scaling}}}}).toJson().at("design").at("scaling"), scaling);
    }
    for (const char* orientation : {"landscape", "portrait", "any"}) {
        EXPECT_EQ(core::AppConfig::fromJson(core::Json{{"orientation", orientation}}).toJson().at("orientation"), orientation);
    }
    EXPECT_EQ(core::AppConfig::fromJson(core::Json::object()).window.title, "Haylen App");
}

TEST(AppConfigTest, RejectsInvalidValues) {
    for (const char* document : {R"([])", R"({"name": 3})", R"({"window": {"width": 0}})", R"({"design": {"scaling": "zoom"}})", R"({"design": {"height": -1}})", R"({"orientation": "sideways"})", R"({"fixedRate": 0})", R"({"clearColor": "blue"})", R"({"title": "typo"})", R"({"window": {"fullScreen": true}})", R"({"design": {"widht": 10}})", R"({"splash": {"background": "blue"}})", R"({"splash": {"image": "logo.png"}})"}) {
        EXPECT_THROW((void)core::AppConfig::fromJson(core::Json::parse(document)), std::invalid_argument) << document;
    }
}

TEST(PluginRegistryTest, FindsPluginsByTypeAndName) {
    plugins::PluginRegistry registry;
    CountingPlugin& plugin = registry.add(std::make_unique<CountingPlugin>());
    EXPECT_EQ(&registry.get<CountingPlugin>(), &plugin);
    EXPECT_EQ(registry.find("counting"), &plugin);
    EXPECT_EQ(registry.find("missing"), nullptr);
    EXPECT_EQ(registry.size(), 1U);
    EXPECT_EQ(registry.getAll().size(), 1U);

    registry.clear();
    EXPECT_EQ(registry.find<CountingPlugin>(), nullptr);
    EXPECT_THROW((void)registry.get<CountingPlugin>(), std::logic_error);
}

TEST(EngineTest, DrivesPluginsScenesAndTimers) {
    std::vector<std::string> log;
    auto scene = std::make_shared<test::RecordingScene>("main", log);
    CountingPlugin* plugin = nullptr;

    // clang-format off
    test::EngineFixture fixture({}, std::make_unique<test::TestApplication>([&](core::Engine& engine) {
        plugin = &engine.addPlugin(std::make_unique<CountingPlugin>());
        engine.getScenes().push(scene);
    }));
    // clang-format on

    int timerCalls = 0;
    fixture.engine().getTimers().after(0.05F, [&] { ++timerCalls; });
    fixture.frames(6, 1.0 / 60.0);

    EXPECT_EQ(log, (std::vector<std::string>{"main:load", "main:enter", "main:enterTransitionFinished"}));
    EXPECT_EQ(scene->updates, 6);
    EXPECT_EQ(scene->renders, 6);
    EXPECT_EQ(scene->uiRenders, 6);
    EXPECT_GE(scene->fixedSteps, 5);
    EXPECT_EQ(timerCalls, 1);
    ASSERT_NE(plugin, nullptr);
    EXPECT_EQ(plugin->started, 1);
    EXPECT_EQ(plugin->frames, 6);
    EXPECT_EQ(plugin->updates, 6);
    EXPECT_EQ(plugin->renders, 6);
    EXPECT_EQ(plugin->uiRenders, 6);
    EXPECT_EQ(plugin->endedFrames, 6);
    EXPECT_GE(plugin->fixedSteps, 5);
    EXPECT_EQ(fixture.engine().getClock().getFrameIndex(), 6U);
    EXPECT_GT(fixture.engine().getRenderer2D().getStats().passes, 0U);

    platform::Event event;
    event.type = platform::Event::Type::KeyDown;
    event.key = input::Key::A;
    fixture.engine().handleEvent(event);
    EXPECT_EQ(plugin->events, 1);
    EXPECT_EQ(log.back(), "main:event");
    EXPECT_TRUE(fixture.engine().getInput().isKeyDown(input::Key::A));
}

TEST(EngineTest, AnnouncesPluginsAddedWhileRunning) {
    test::EngineFixture fixture;
    std::vector<std::string> started;
    fixture.engine().getEvents().on(core::LifecycleEvent::kPluginStarted, [&](core::EventBus::Event& event) { started.push_back(event.getData().at("name").get<std::string>()); });
    const CountingPlugin& plugin = fixture.engine().addPlugin(std::make_unique<CountingPlugin>());
    EXPECT_EQ(plugin.started, 1);
    EXPECT_EQ(started, std::vector<std::string>{"counting"});
}

TEST(EngineTest, TracksAppStatesFromPlatformEvents) {
    test::EngineFixture fixture;
    core::Engine& engine = fixture.engine();
    std::vector<std::string> log;
    const core::ScopedConnection states = engine.appStateChanged.connect([&](core::Engine::AppState state) { log.push_back("state " + std::to_string(static_cast<int>(state))); });
    const core::ScopedConnection resized = engine.resized.connect([&] { log.emplace_back("resized"); });
    const core::ScopedConnection quit = engine.quitRequested.connect([&] { log.emplace_back("quit"); });
    for (const std::string_view name : {core::LifecycleEvent::kAppActive, core::LifecycleEvent::kAppInactive, core::LifecycleEvent::kAppBackground, core::LifecycleEvent::kWindowResized, core::LifecycleEvent::kWindowFocusLost, core::LifecycleEvent::kWindowFocusGained, core::LifecycleEvent::kAppQuitRequested}) {
        engine.getEvents().on(name, [&log, name](core::EventBus::Event& event) { log.push_back(std::string(name) + (event.getData().is_null() ? "" : " " + event.getData().dump())); });
    }

    engine.getInput().handleEvent({.type = platform::Event::Type::KeyDown, .key = input::Key::A}, engine.getViewport());
    engine.getVirtualInput().setButton("jump", true);
    const int persisted = fixture.host().getPersistCount();
    for (const platform::Event::Type type : {platform::Event::Type::FocusLost, platform::Event::Type::FocusGained, platform::Event::Type::Suspended, platform::Event::Type::FocusGained, platform::Event::Type::Resumed, platform::Event::Type::Resized, platform::Event::Type::QuitRequested}) {
        engine.handleEvent({.type = type});
    }
    EXPECT_EQ(log, (std::vector<std::string>{"windowFocusLost", "state 1", "appInactive", "windowFocusGained", "state 0", "appActive", "state 2", "appBackground", "windowFocusGained", "state 0", "appActive", "resized", R"(windowResized {"height":1080.0,"width":1920.0})", "quit", "appQuitRequested"}));
    EXPECT_FALSE(engine.getInput().isKeyDown(input::Key::A));
    EXPECT_FALSE(engine.getVirtualInput().isButtonDown("jump"));
    EXPECT_EQ(fixture.host().getPersistCount(), persisted + 1) << "going to the background makes the files of the app durable";
    EXPECT_EQ(engine.getAppState(), core::Engine::AppState::Active);
}

TEST(EngineTest, TreatsInterruptionsAsInactiveTime) {
    test::EngineFixture fixture;
    core::Engine& engine = fixture.engine();
    std::vector<core::Engine::AppState> states;
    const core::ScopedConnection connection = engine.appStateChanged.connect([&](core::Engine::AppState state) { states.push_back(state); });

    // A phone call keeps the app inactive even when its window gets the focus back, and going away and back keeps the interruption until it ends.
    for (const platform::Event::Type type : {platform::Event::Type::InterruptionBegan, platform::Event::Type::FocusLost, platform::Event::Type::FocusGained, platform::Event::Type::Suspended, platform::Event::Type::Resumed}) {
        engine.handleEvent({.type = type});
    }
    EXPECT_EQ(engine.getAppState(), core::Engine::AppState::Inactive);
    engine.handleEvent({.type = platform::Event::Type::InterruptionEnded});
    EXPECT_EQ(engine.getAppState(), core::Engine::AppState::Active);
    EXPECT_EQ(states, (std::vector{core::Engine::AppState::Inactive, core::Engine::AppState::Background, core::Engine::AppState::Inactive, core::Engine::AppState::Active}));

    // An interruption that ends while the window has no focus leaves the app inactive until the focus returns.
    engine.handleEvent({.type = platform::Event::Type::FocusLost});
    engine.handleEvent({.type = platform::Event::Type::InterruptionBegan});
    engine.handleEvent({.type = platform::Event::Type::InterruptionEnded});
    EXPECT_EQ(engine.getAppState(), core::Engine::AppState::Inactive);
    engine.handleEvent({.type = platform::Event::Type::FocusGained});
    EXPECT_EQ(engine.getAppState(), core::Engine::AppState::Active);
}

TEST(EngineTest, PublishesTheKeyboardAndTheNetwork) {
    test::EngineFixture fixture({{"app.json", R"({"name": "Test App", "identifier": "dev.haylen.tests", "design": {"width": 960, "height": 540, "scaling": "fit"}})"}});
    core::Engine& engine = fixture.engine();
    std::vector<std::string> log;
    for (const std::string_view name : {core::LifecycleEvent::kKeyboardShown, core::LifecycleEvent::kKeyboardHidden, core::LifecycleEvent::kNetworkOnline, core::LifecycleEvent::kNetworkOffline}) {
        engine.getEvents().on(name, [&log, name](core::EventBus::Event& event) { log.push_back(std::string(name) + (event.getData().is_null() ? "" : " " + event.getData().dump())); });
    }

    // The keyboard frame reaches the app in design units, and only changes are published.
    fixture.frames(1);
    engine.handleEvent({.type = platform::Event::Type::KeyboardChanged, .keyboardFrame = {0.0F, 1080.0F, 0.0F, 0.0F}});
    engine.handleEvent({.type = platform::Event::Type::KeyboardChanged, .keyboardFrame = {0.0F, 600.0F, 1920.0F, 480.0F}});
    engine.handleEvent({.type = platform::Event::Type::KeyboardChanged, .keyboardFrame = {0.0F, 600.0F, 1920.0F, 480.0F}});
    engine.handleEvent({.type = platform::Event::Type::KeyboardChanged});
    engine.handleEvent({.type = platform::Event::Type::KeyboardChanged});

    // The network is unknown until the platform reports it, and the first report publishes the state the app starts in.
    EXPECT_EQ(engine.getNetworkState(), core::Engine::NetworkState::Unknown);
    for (const bool online : {true, true, false, false, true}) {
        engine.handleEvent({.type = platform::Event::Type::NetworkChanged, .online = online});
    }
    EXPECT_EQ(engine.getNetworkState(), core::Engine::NetworkState::Online);
    EXPECT_EQ(log, (std::vector<std::string>{R"(keyboardShown {"height":240.0,"width":960.0,"x":0.0,"y":300.0})", "keyboardHidden", "networkOnline", "networkOffline", "networkOnline"}));
}

TEST(EngineTest, PublishesWindowAndGamepadChanges) {
    test::EngineFixture fixture;
    core::Engine& engine = fixture.engine();
    std::vector<std::string> log;
    for (const std::string_view name : {core::LifecycleEvent::kWindowFullscreenChanged, core::LifecycleEvent::kWindowOrientationChanged, core::LifecycleEvent::kWindowSafeAreaChanged, core::LifecycleEvent::kGamepadConnected, core::LifecycleEvent::kGamepadDisconnected}) {
        engine.getEvents().on(name, [&log, name](core::EventBus::Event& event) { log.push_back(std::string(name) + " " + event.getData().dump()); });
    }
    fixture.host().setGamepad(1, {.connected = true, .name = "Pad"});
    fixture.frames(2);
    EXPECT_EQ(log, (std::vector<std::string>{R"(gamepadConnected {"gamepad":2,"name":"Pad"})"}));

    log.clear();
    fixture.host().setFullscreen(true);
    fixture.host().setSafeAreaInsets({.top = 108.0F});
    fixture.host().setGamepad(1, {});
    fixture.frames(2);
    EXPECT_EQ(log, (std::vector<std::string>{R"(windowFullscreenChanged {"fullscreen":true})", R"(windowSafeAreaChanged {"height":972.0,"width":1920.0,"x":0.0,"y":108.0})", R"(gamepadDisconnected {"gamepad":2,"name":"Pad"})"}));

    log.clear();
    fixture.host().setSafeAreaInsets({});
    fixture.host().resize({1080.0F, 1920.0F});
    fixture.frames(1);
    ASSERT_EQ(log.size(), 2U);
    EXPECT_EQ(log[0], R"(windowOrientationChanged {"orientation":"portrait"})");
    EXPECT_EQ(log[1].rfind("windowSafeAreaChanged", 0), 0U);
}

TEST(EngineTest, KeepsHeldTouchesOnTheirScreenPointAcrossAResize) {
    test::EngineFixture fixture;
    core::Engine& engine = fixture.engine();
    platform::Event began;
    began.type = platform::Event::Type::TouchBegan;
    began.touchCount = 1;
    began.touches[0] = {.id = 1, .position = {400.0F, 300.0F}, .changed = true};
    engine.handleEvent(began);
    fixture.frames(1);

    fixture.host().resize({1080.0F, 1920.0F});
    fixture.frames(1);
    const math::Vec2 expected = engine.getViewport().toDesign({400.0F, 300.0F});
    ASSERT_EQ(engine.getInput().getTouches().size(), 1U);
    EXPECT_NEAR(engine.getInput().getTouches()[0].position.x, expected.x, 1e-3F);
    EXPECT_NEAR(engine.getInput().getTouches()[0].position.y, expected.y, 1e-3F);
}

TEST(EngineTest, HaltsUpdatesByTheLifecycleOptions) {
    test::EngineFixture fixture({{"app.json", R"({"name": "Test App", "identifier": "dev.haylen.tests", "lifecycle": {"pauseOnFocusLoss": true, "muteOnFocusLoss": true}})"}});
    core::Engine& engine = fixture.engine();
    EXPECT_TRUE(engine.getLifecycle().pauseOnBackground);
    EXPECT_TRUE(engine.getLifecycle().pauseOnFocusLoss);
    int ticks = 0;
    engine.getTimers().every(0.05F, [&] { ++ticks; });

    engine.handleEvent({.type = platform::Event::Type::FocusLost});
    EXPECT_TRUE(engine.isHalted());
    EXPECT_TRUE(engine.getAudio().isBusMuted("master"));
    fixture.frames(5);
    EXPECT_EQ(ticks, 0);

    // Coming back does not jump the clock by the time the app was away.
    engine.handleEvent({.type = platform::Event::Type::FocusGained});
    EXPECT_FALSE(engine.getAudio().isBusMuted("master"));
    fixture.frames(1, 10.0);
    EXPECT_EQ(ticks, 0);
    fixture.frames(1, 0.06);
    EXPECT_EQ(ticks, 1);

    engine.handleEvent({.type = platform::Event::Type::Suspended});
    EXPECT_TRUE(engine.isHalted());
    const std::uint64_t passes = engine.getRenderer2D().getStats().passes;
    fixture.frames(3);
    EXPECT_EQ(engine.getRenderer2D().getStats().passes, passes);
    engine.setLifecycle({.pauseOnBackground = false});
    EXPECT_FALSE(engine.isHalted());
    fixture.frames(1, 0.06);
    fixture.frames(1, 0.06);
    EXPECT_EQ(ticks, 2);
    engine.handleEvent({.type = platform::Event::Type::Resumed});
    EXPECT_FALSE(engine.isHalted());
}

TEST(EngineTest, PausesTheGame) {
    std::vector<std::string> log;
    test::EngineFixture fixture;
    core::Engine& engine = fixture.engine();
    const core::ScopedConnection paused = engine.pausedChanged.connect([&](bool value) { log.push_back(value ? "paused" : "resumed"); });
    engine.getEvents().on(core::LifecycleEvent::kPaused, [&](core::EventBus::Event&) { log.emplace_back("event paused"); });
    engine.getEvents().on(core::LifecycleEvent::kUnpaused, [&](core::EventBus::Event&) { log.emplace_back("event unpaused"); });
    auto game = std::make_shared<test::RecordingScene>("game", log);
    engine.getScenes().push(game);
    fixture.frames(1);

    engine.setPaused(true);
    engine.setPaused(true);
    EXPECT_TRUE(engine.isPaused());
    const int fixedBefore = game->fixedSteps;
    fixture.frames(3);
    EXPECT_EQ(game->updates, 1);
    EXPECT_EQ(game->fixedSteps, fixedBefore);
    EXPECT_EQ(game->renders, 4);

    engine.setPaused(false);
    fixture.frames(1);
    EXPECT_EQ(game->updates, 2);
    EXPECT_EQ(log, (std::vector<std::string>{"game:load", "game:enter", "game:enterTransitionFinished", "game:paused", "paused", "event paused", "game:unpaused", "resumed", "event unpaused"}));
}

TEST(EngineTest, ReportsLowMemoryToTheApp) {
    test::EngineFixture fixture({{"content/kept.json", "{}"}, {"content/dropped.json", "[]"}});
    assets::Manager& assets = fixture.engine().getAssets();
    const std::shared_ptr<void> kept = assets.load("json", "kept.json");
    (void)assets.load("json", "dropped.json");
    EXPECT_EQ(assets.getCachedCount(), 1U);

    int warnings = 0;
    const core::ScopedConnection lowMemory = fixture.engine().lowMemory.connect([&] { ++warnings; });
    fixture.runLua("warnings = 0 require('haylen.scene').push({event = function(_, event) if event.type == 'lowMemory' then warnings = warnings + 1 end end})");
    fixture.frames(1);
    platform::Event event;
    event.type = platform::Event::Type::LowMemory;
    fixture.engine().handleEvent(event);
    EXPECT_EQ(warnings, 1);
    EXPECT_EQ(fixture.lua("return warnings"), "1");
    EXPECT_EQ(assets.releaseUnused(), 0U) << "the warning already pruned the entries of released assets";
}

TEST(EngineTest, ReportsErrorsAndKeepsRendering) {
    std::vector<std::string> log;
    auto scene = std::make_shared<test::RecordingScene>("main", log);
    // clang-format off
    test::EngineFixture fixture({}, std::make_unique<test::TestApplication>([&](core::Engine& engine) {
        engine.getScenes().push(scene);
    }));
    // clang-format on

    std::string reported;
    const core::ScopedConnection connection = fixture.engine().errorRaised.connect([&](const lua::Error& error) { reported = error.what(); });
    fixture.frames(1);
    fixture.engine().reportError("first failure");
    fixture.engine().reportError("second failure");
    fixture.frames(3);

    EXPECT_EQ(reported, "first failure");
    ASSERT_NE(fixture.engine().getError(), nullptr);
    EXPECT_STREQ(fixture.engine().getError()->what(), "first failure");
    EXPECT_EQ(scene->updates, 1);
    EXPECT_GT(fixture.engine().getRenderer2D().getStats().sprites, 0U);
}

TEST(EngineTest, TurnsStartupFailuresIntoErrors) {
    {
        test::EngineFixture failing({}, std::make_unique<test::TestApplication>([](core::Engine&) { throw std::runtime_error("cannot start"); }));
        ASSERT_NE(failing.engine().getError(), nullptr);
        EXPECT_STREQ(failing.engine().getError()->what(), "cannot start");
    }

    test::EngineFixture broken({{"source/main.lua", "this is not lua"}});
    ASSERT_NE(broken.engine().getError(), nullptr);
    EXPECT_NE(std::string_view(broken.engine().getError()->what()).find("source/main.lua"), std::string::npos);
}

TEST(EngineTest, ReportsFailuresFromAsynchronousCallbacksAndRendering) {
    test::EngineFixture fixture;
    fixture.engine().getJobs().postToFrame([] { throw std::runtime_error("late failure"); });
    fixture.frames(1);
    ASSERT_NE(fixture.engine().getError(), nullptr);
    EXPECT_STREQ(fixture.engine().getError()->what(), "late failure");
}

TEST(EngineTest, ReportsFailuresOfPluginsAtTheEndOfAFrame) {
    test::EngineFixture fixture({}, std::make_unique<test::TestApplication>([](core::Engine& engine) { engine.addPlugin(std::make_unique<FailingEndPlugin>()); }));
    fixture.frames(1);
    ASSERT_NE(fixture.engine().getError(), nullptr);
    EXPECT_STREQ(fixture.engine().getError()->what(), "end of frame failed");
}

TEST(EngineTest, ShowsErrorsOfAsyncTasksWithTheirStack) {
    test::EngineFixture fixture({{"source/scenes/loader.lua", "return function()\n    require('async').sleep(1):await()\n    error('the island failed to load')\nend"}});
    fixture.runLua("require('async').spawn(require('scenes.loader'))");
    EXPECT_EQ(fixture.engine().getError(), nullptr);
    ASSERT_TRUE(fixture.frameUntil([&] { return fixture.engine().getError() != nullptr; }));
    const lua::Error& error = *fixture.engine().getError();
    EXPECT_EQ(error.getMessage(), "the island failed to load");
    EXPECT_EQ(error.getFile(), "source/scenes/loader.lua");
    EXPECT_EQ(error.getLine(), 3);

    // The task shows its own frames only: neither the xpcall of the engine nor the task wrapper and coroutine entry below it.
    ASSERT_EQ(error.getFrames().size(), 2U);
    EXPECT_EQ(error.getFrames()[0].source, "[C]");
    EXPECT_EQ(error.getFrames()[0].function, "global 'error'");
    EXPECT_EQ(error.getFrames()[0].kind, lua::Error::Frame::Kind::C);
    EXPECT_EQ(error.getFrames()[1].source, "source/scenes/loader.lua");
    EXPECT_EQ(error.getFrames()[1].line, 3);
    EXPECT_EQ(error.getFrames()[1].function, "function <source/scenes/loader.lua:1>");
    EXPECT_EQ(error.getFrames()[1].kind, lua::Error::Frame::Kind::Lua);
    EXPECT_NE(fixture.lua("require('async').spawn(3)").find("bad argument #1 to 'spawn' (function expected, got number)"), std::string::npos);
}

TEST(EngineTest, ReportsRenderSubmissionFailures) {
    test::EngineFixture fixture;
    fixture.host().resize({20000.0F, 20000.0F});
    fixture.engine().getScenes().push(std::make_shared<test::DrawingScene>([](core::Engine& engine) { engine.getRenderer2D().beginWorld(graphics2d::Camera{}, {.ambientLight = math::Color::black()}); }));
    fixture.frames(2);
    ASSERT_NE(fixture.engine().getError(), nullptr);
    EXPECT_NE(std::string_view(fixture.engine().getError()->what()).find("Texture dimensions exceed the device limit"), std::string::npos);

    fixture.host().resize({1024.0F, 768.0F});
    fixture.frames(1);
    EXPECT_GT(fixture.engine().getRenderer2D().getStats().sprites, 0U);
}

TEST(EngineTest, AllowsOnlyOneEngineAtATime) {
    test::EngineFixture running;
    EXPECT_THROW(test::EngineFixture second, std::logic_error);
    running.frames(1);
}

TEST(EngineTest, QuitStopsFramesAndAsksTheHost) {
    test::EngineFixture fixture;
    EXPECT_TRUE(fixture.engine().isRunning());
    fixture.engine().quit();
    EXPECT_FALSE(fixture.engine().isRunning());
    EXPECT_TRUE(fixture.host().isQuitRequested());

    const std::uint64_t before = fixture.engine().getClock().getFrameIndex();
    fixture.frames(3);
    EXPECT_EQ(fixture.engine().getClock().getFrameIndex(), before);
}

TEST(EngineTest, ExposesServicesAndPlatformName) {
    test::EngineFixture fixture;
    core::Engine& engine = fixture.engine();
    EXPECT_EQ(engine.getPlatformName(), "headless");
    EXPECT_EQ(engine.getConfig().identifier, "dev.haylen.tests");
    EXPECT_EQ(&engine.getWindow(), &fixture.host());
    EXPECT_EQ(engine.getPackage().getName(), "test");
    EXPECT_NE(engine.getLuaState(), nullptr);
    EXPECT_NE(engine.getDefaultFont(), nullptr);
    EXPECT_NE(engine.getPlugins().find("core"), nullptr);
    EXPECT_NE(engine.getPlugins().find("graphics2d"), nullptr);
    EXPECT_NE(engine.getPlugins().find("physics2d"), nullptr);
    EXPECT_NE(engine.getPlugins().find("assets"), nullptr);
}

TEST(EngineTest, ConstructorRejectsMissingParts) {
    test::TemporaryDirectory directory;
    platform::HeadlessHost host(directory.getPath());
    EXPECT_THROW(core::Engine(host, nullptr, core::AppConfig{}, std::make_unique<lua::Application>()), std::invalid_argument);
    EXPECT_THROW(core::Engine(host, std::make_shared<io::MemoryPackage>("empty"), core::AppConfig{}, nullptr), std::invalid_argument);
}

} // namespace haylen::core
