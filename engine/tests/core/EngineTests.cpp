#include <gtest/gtest.h>
#include <imgui_internal.h>

#include <chrono>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "gpu/NullDevice.hpp"
#include "haylen/2d/graphics/Camera.hpp"
#include "haylen/2d/graphics/Renderer.hpp"
#include "haylen/assets/Manager.hpp"
#include "haylen/core/AppConfig.hpp"
#include "haylen/core/AppError.hpp"
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
#include "haylen/math/Rect.hpp"
#include "haylen/math/Vec2.hpp"
#include "haylen/platform/Event.hpp"
#include "haylen/plugins/Plugin.hpp"
#include "haylen/plugins/UiPlugin.hpp"
#include "haylen/storage/UserStorage.hpp"
#include "platform/LinkRelay.hpp"
#include "platform/NativeViews.hpp"
#include "platform/headless/HeadlessHost.hpp"
#include "support/AllocationTracker.hpp"
#include "support/DrawingScene.hpp"
#include "support/EngineFixture.hpp"
#include "support/RecordingScene.hpp"
#include "support/TemporaryDirectory.hpp"
#include "support/TestApplication.hpp"
#include "support/TestFiles.hpp"

namespace haylen::core {

class EngineTest : public ::testing::Test {
  protected:
    // Fails in one hook of every frame, like a plugin with a bug in its bookkeeping.
    class FailingFramePlugin final : public plugins::Plugin {
      public:
        enum class Hook {
            Event,
            Overlay,
            EndFrame,
        };

        explicit FailingFramePlugin(Hook value) : failing(value) {}

        [[nodiscard]] std::string_view getName() const noexcept override {
            return "failing";
        }
        void event(Engine&, const platform::Event&) override {
            fail(Hook::Event, "event failed");
        }
        void renderOverlay(Engine&) override {
            fail(Hook::Overlay, "overlay failed");
        }
        void endFrame(Engine&) override {
            fail(Hook::EndFrame, "end of frame failed");
        }

      private:
        void fail(Hook hook, const char* message) const {
            if (hook == failing) {
                throw std::runtime_error(message);
            }
        }

        Hook failing;
    };

    // Fails to start, and counts the stops it receives, which a plugin that never started must not get.
    class FailingStartPlugin final : public plugins::Plugin {
      public:
        explicit FailingStartPlugin(int& stopCount) : stops(stopCount) {}

        [[nodiscard]] std::string_view getName() const noexcept override {
            return "failingStart";
        }
        void start(Engine&) override {
            throw std::runtime_error("start failed");
        }
        void stop(Engine&) override {
            ++stops;
        }

      private:
        int& stops;
    };

    class CountingPlugin final : public plugins::Plugin {
      public:
        [[nodiscard]] std::string_view getName() const noexcept override {
            return "counting";
        }
        void start(Engine&) override {
            ++started;
        }
        void stop(Engine&) override {
            ++stopped;
        }
        void event(Engine&, const platform::Event&) override {
            ++events;
        }
        void beginFrame(Engine&, float) override {
            ++frames;
        }
        void fixedUpdate(Engine&, float) override {
            ++fixedSteps;
        }
        void update(Engine&, float) override {
            ++updates;
        }
        void render(Engine&) override {
            ++renders;
        }
        void renderUi(Engine&, const SceneView& view) override {
            uiRenders += view.current ? 1 : 0;
        }
        void endFrame(Engine&) override {
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
};

TEST_F(EngineTest, FindsPluginsByTypeAndName) {
    plugins::PluginRegistry registry;
    CountingPlugin& plugin = registry.add(std::make_unique<CountingPlugin>());
    EXPECT_EQ(&registry.get<CountingPlugin>(), &plugin);
    EXPECT_EQ(registry.find("counting"), &plugin);
    EXPECT_EQ(registry.find("missing"), nullptr);
    EXPECT_EQ(registry.size(), 1U);
    EXPECT_EQ(registry.getAll().size(), 1U);

    registry.clear();
    EXPECT_EQ(registry.find<CountingPlugin>(), nullptr);

    try {
        (void)registry.get<CountingPlugin>();
        FAIL() << "The registry has no plugin left.";
    } catch (const std::logic_error& error) {
        EXPECT_STREQ(error.what(), "The engine has no plugin of the requested type. Add it with \"Engine::addPlugin\" before code asks for it.");
    }
}

TEST_F(EngineTest, DrivesPluginsScenesAndTimers) {
    std::vector<std::string> log;
    auto scene = std::make_shared<test::RecordingScene>("main", log);
    CountingPlugin* plugin = nullptr;

    // clang-format off
    test::EngineFixture fixture({}, std::make_unique<test::TestApplication>([&](Engine& engine) {
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

// The frame loop reuses its storage, so only the scripts allocate, through the allocator of Lua.
TEST_F(EngineTest, SteadyFramesAllocateNothing) {
    test::EngineFixture fixture;
    // clang-format off
    fixture.runLua(R"(
        box = {x = 0}
        require('haylen.scene').push({update = function(self, dt) box.x = box.x + dt end, render = function() end})
        require('haylen.timer').every(0.01, function() end)
        require('haylen.tween').to(box, 100, {x = 5})
    )");
    // clang-format on

    fixture.frames(10);
    const test::AllocationTracker tracker;
    fixture.frames(30);
    EXPECT_EQ(tracker.getTotal(), 0U);
}

TEST_F(EngineTest, AnnouncesPluginsAddedWhileRunning) {
    test::EngineFixture fixture;
    std::vector<std::string> started;
    fixture.engine().getEvents().on(LifecycleEvent::kPluginStarted, [&](EventBus::Event& event) { started.push_back(event.getData().at("name").get<std::string>()); });
    const CountingPlugin& plugin = fixture.engine().addPlugin(std::make_unique<CountingPlugin>());
    EXPECT_EQ(plugin.started, 1);
    EXPECT_EQ(started, std::vector<std::string>{"counting"});
}

// A plugin that fails to start leaves the plugins after it unstarted, and the frames that show the error screen never reach them.
TEST_F(EngineTest, KeepsShowingTheErrorOfAPluginThatFailedToStart) {
    test::EngineFixture fixture({{"haylen.json", R"({"localization": {"folder": "locale", "fallback": "de"}})"}, {"content/locale/en.json", "{}"}});
    ASSERT_NE(fixture.engine().getError(), nullptr);
    fixture.frames(3);
    EXPECT_TRUE(fixture.engine().isRunning());
    EXPECT_NE(fixture.engine().getError()->getMessage().find("localization.fallback"), std::string::npos);
}

TEST_F(EngineTest, TracksAppStatesFromPlatformEvents) {
    test::EngineFixture fixture;
    Engine& engine = fixture.engine();
    std::vector<std::string> log;
    const ScopedConnection states(engine.appStateChanged.connect([&](Engine::AppState state) { log.push_back("state " + std::to_string(static_cast<int>(state))); }));
    const ScopedConnection resized(engine.resized.connect([&] { log.emplace_back("resized"); }));
    const ScopedConnection quit(engine.quitRequested.connect([&] { log.emplace_back("quit"); }));

    for (const std::string_view name : {LifecycleEvent::kAppActive, LifecycleEvent::kAppInactive, LifecycleEvent::kAppBackground, LifecycleEvent::kWindowResized, LifecycleEvent::kWindowFocusLost, LifecycleEvent::kWindowFocusGained, LifecycleEvent::kAppQuitRequested}) {
        engine.getEvents().on(name, [&log, name](EventBus::Event& event) { log.push_back(std::string(name) + (event.getData().is_null() ? "" : " " + event.getData().dump())); });
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
    EXPECT_EQ(fixture.host().getPersistCount(), persisted + 1) << "Going to the background makes the files of the app durable.";
    EXPECT_EQ(engine.getAppState(), Engine::AppState::Active);
}

TEST_F(EngineTest, TreatsInterruptionsAsInactiveTime) {
    test::EngineFixture fixture;
    Engine& engine = fixture.engine();
    std::vector<Engine::AppState> states;
    const ScopedConnection connection(engine.appStateChanged.connect([&](Engine::AppState state) { states.push_back(state); }));

    // A phone call keeps the app inactive even when its window gets the focus back, and going away and back keeps the interruption until it ends.
    for (const platform::Event::Type type : {platform::Event::Type::InterruptionBegan, platform::Event::Type::FocusLost, platform::Event::Type::FocusGained, platform::Event::Type::Suspended, platform::Event::Type::Resumed}) {
        engine.handleEvent({.type = type});
    }

    EXPECT_EQ(engine.getAppState(), Engine::AppState::Inactive);
    engine.handleEvent({.type = platform::Event::Type::InterruptionEnded});
    EXPECT_EQ(engine.getAppState(), Engine::AppState::Active);
    EXPECT_EQ(states, (std::vector{Engine::AppState::Inactive, Engine::AppState::Background, Engine::AppState::Inactive, Engine::AppState::Active}));

    // An interruption that ends while the window has no focus leaves the app inactive until the focus returns.
    engine.handleEvent({.type = platform::Event::Type::FocusLost});
    engine.handleEvent({.type = platform::Event::Type::InterruptionBegan});
    engine.handleEvent({.type = platform::Event::Type::InterruptionEnded});
    EXPECT_EQ(engine.getAppState(), Engine::AppState::Inactive);
    engine.handleEvent({.type = platform::Event::Type::FocusGained});
    EXPECT_EQ(engine.getAppState(), Engine::AppState::Active);
}

TEST_F(EngineTest, ComesBackInactiveWhileTheWindowLacksTheFocus) {
    test::EngineFixture fixture;
    Engine& engine = fixture.engine();

    // A dialog took the focus of the window before the app went to the background and still has it when the app comes back.
    for (const platform::Event::Type type : {platform::Event::Type::FocusLost, platform::Event::Type::Suspended, platform::Event::Type::Resumed}) {
        engine.handleEvent({.type = type});
    }

    EXPECT_EQ(engine.getAppState(), Engine::AppState::Inactive);
    engine.handleEvent({.type = platform::Event::Type::FocusGained});
    EXPECT_EQ(engine.getAppState(), Engine::AppState::Active);
}

TEST_F(EngineTest, PublishesTheKeyboardAndTheNetwork) {
    test::EngineFixture fixture({{"haylen.json", R"({"name": "Test App", "identifier": "com.haylen.tests", "design": {"width": 960, "height": 540, "scaling": "fit"}})"}});
    Engine& engine = fixture.engine();
    std::vector<std::string> log;
    for (const std::string_view name : {LifecycleEvent::kKeyboardShown, LifecycleEvent::kKeyboardHidden, LifecycleEvent::kNetworkOnline, LifecycleEvent::kNetworkOffline}) {
        engine.getEvents().on(name, [&log, name](EventBus::Event& event) { log.push_back(std::string(name) + (event.getData().is_null() ? "" : " " + event.getData().dump())); });
    }

    // The keyboard frame reaches the app in design units, and only changes are published.
    fixture.frames(1);
    engine.handleEvent({.type = platform::Event::Type::KeyboardChanged, .keyboardFrame = {0.0F, 1080.0F, 0.0F, 0.0F}});
    engine.handleEvent({.type = platform::Event::Type::KeyboardChanged, .keyboardFrame = {0.0F, 600.0F, 1920.0F, 480.0F}});
    engine.handleEvent({.type = platform::Event::Type::KeyboardChanged, .keyboardFrame = {0.0F, 600.0F, 1920.0F, 480.0F}});
    engine.handleEvent({.type = platform::Event::Type::KeyboardChanged});
    engine.handleEvent({.type = platform::Event::Type::KeyboardChanged});

    // The network is unknown until the platform reports it, and the first report publishes the state the app starts in.
    EXPECT_EQ(engine.getNetworkState(), Engine::NetworkState::Unknown);

    for (const bool online : {true, true, false, false, true}) {
        engine.handleEvent({.type = platform::Event::Type::NetworkChanged, .online = online});
    }

    EXPECT_EQ(engine.getNetworkState(), Engine::NetworkState::Online);
    EXPECT_EQ(log, (std::vector<std::string>{R"(keyboardShown {"height":240.0,"width":960.0,"x":0.0,"y":300.0})", "keyboardHidden", "networkOnline", "networkOffline", "networkOnline"}));
}

TEST_F(EngineTest, PublishesWindowAndGamepadChanges) {
    test::EngineFixture fixture;
    Engine& engine = fixture.engine();
    std::vector<std::string> log;
    for (const std::string_view name : {LifecycleEvent::kWindowFullscreenChanged, LifecycleEvent::kWindowOrientationChanged, LifecycleEvent::kWindowSafeAreaChanged, LifecycleEvent::kGamepadConnected, LifecycleEvent::kGamepadDisconnected}) {
        engine.getEvents().on(name, [&log, name](EventBus::Event& event) { log.push_back(std::string(name) + " " + event.getData().dump()); });
    }

    fixture.host().setGamepad(1, {.connected = true, .name = "Pad"});
    fixture.frames(2);
    EXPECT_EQ(log, (std::vector<std::string>{R"(gamepadConnected {"gamepad":2,"name":"Pad"})"}));

    log.clear();
    fixture.host().setFullscreen(true, [](const platform::SystemAnswer&) {});
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

// A window without area, such as a canvas that its page hides, draws nothing, lays nothing out and hands no size to the GPU device or to ImGui, while the app runs on, and draws at the right size once it has an area again.
TEST_F(EngineTest, DrawsNothingWhileTheWindowHasNoArea) {
    test::EngineFixture fixture({{"source/main.lua", "count = 0 sizes = {} require('haylen.events').on('windowResized', function(event) sizes[#sizes + 1] = string.format('%dx%d', event.width, event.height) end) require('haylen.ui').mount(require('haylen.ui').column{require('haylen.ui').button{text = 'Play'}}) require('haylen.scene').push({update = function() count = count + 1 end})"}});
    Engine& engine = fixture.engine();
    const ImGuiContext& imgui = *engine.getPlugin<plugins::UiPlugin>().getBackend().getImGuiContext();
    platform::Event resized;
    resized.type = platform::Event::Type::Resized;
    fixture.frames(2);
    const int drawnFrames = imgui.FrameCount;
    const math::Rect mapped = engine.getViewport().getPixelRect();
    const std::string before = fixture.lua("return count");

    fixture.host().resize({0.0F, 0.0F});
    engine.handleEvent(resized);
    fixture.frames(3);
    EXPECT_FALSE(engine.isDrawing());
    EXPECT_EQ(imgui.FrameCount, drawnFrames);
    EXPECT_EQ(engine.getViewport().getPixelRect(), mapped);
    EXPECT_EQ(fixture.host().getGpu().getSurfaceSize().width, 1920);
    EXPECT_EQ(fixture.lua("return #sizes"), "0");
    EXPECT_EQ(fixture.lua("return count - " + before), "3");

    fixture.host().resize({1280.0F, 720.0F});
    engine.handleEvent(resized);
    fixture.frames(1);
    EXPECT_TRUE(engine.isDrawing());
    EXPECT_EQ(imgui.FrameCount, drawnFrames + 1);
    EXPECT_EQ(engine.getViewport().getPixelRect().getSize(), (math::Vec2{1280.0F, 720.0F}));
    EXPECT_EQ(fixture.host().getGpu().getSurfaceSize().width, 1280);
    EXPECT_EQ(fixture.lua("return table.concat(sizes, ',')"), "1280x720");
}

TEST_F(EngineTest, KeepsHeldTouchesOnTheirScreenPointAcrossAResize) {
    test::EngineFixture fixture;
    Engine& engine = fixture.engine();
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

// An app of any orientation turns its design with the screen, so a phone turned on its side keeps the size of the design units and lays out wide.
TEST_F(EngineTest, TurnsTheDesignOfAnAppOfAnyOrientationWithTheScreen) {
    test::EngineFixture fixture({{"haylen.json", R"({"name": "Test App", "identifier": "com.haylen.tests", "orientation": "any", "design": {"width": 440, "height": 956, "scaling": "expand"}})"}});
    Engine& engine = fixture.engine();
    fixture.host().resize({1320.0F, 2868.0F});
    fixture.frames(1);
    EXPECT_EQ(engine.getViewport().getDesignSize(), math::Vec2(440.0F, 956.0F));
    const math::Vec2 portrait = engine.getViewport().getPixelsPerUnit();

    fixture.host().resize({2868.0F, 1320.0F});
    fixture.frames(1);
    EXPECT_EQ(engine.getViewport().getDesignSize(), math::Vec2(956.0F, 440.0F));
    EXPECT_EQ(engine.getViewport().getPixelsPerUnit(), portrait);
    EXPECT_EQ(engine.getViewport().getVisibleRect(), math::Rect(0.0F, 0.0F, 956.0F, 440.0F));
    EXPECT_EQ(fixture.lua("local width, height = require('haylen.viewport').designSize() return width .. ' ' .. height"), "956.0 440.0");
}

TEST_F(EngineTest, KeepsTheDesignOfAnAppOfOneOrientation) {
    test::EngineFixture fixture({{"haylen.json", R"({"name": "Test App", "identifier": "com.haylen.tests", "orientation": "landscape", "design": {"width": 1920, "height": 1080}})"}});
    fixture.host().resize({1080.0F, 1920.0F});
    fixture.frames(1);
    EXPECT_EQ(fixture.engine().getViewport().getDesignSize(), math::Vec2(1920.0F, 1080.0F));
}

TEST_F(EngineTest, HaltsUpdatesByTheLifecycleOptions) {
    test::EngineFixture fixture({{"haylen.json", R"({"name": "Test App", "identifier": "com.haylen.tests", "lifecycle": {"pauseOnFocusLoss": true, "muteOnFocusLoss": true}})"}});
    Engine& engine = fixture.engine();
    EXPECT_TRUE(engine.getLifecycle().pauseOnBackground);
    EXPECT_TRUE(engine.getLifecycle().pauseOnFocusLoss);
    int ticks = 0;
    engine.getTimers().every(0.05F, [&] { ++ticks; });

    engine.handleEvent({.type = platform::Event::Type::FocusLost});
    EXPECT_TRUE(engine.isHalted());
    EXPECT_TRUE(engine.getAudio().isOutputMuted());
    EXPECT_FALSE(engine.getAudio().isBusMuted("master")) << "What the app saves of its mute stays its own choice.";
    EXPECT_EQ(fixture.lua("return tostring(require('haylen.audio').outputMuted()) .. ' ' .. tostring(require('haylen.audio').busMuted('master'))"), "true false");
    fixture.frames(5);
    EXPECT_EQ(ticks, 0);

    // Coming back does not jump the clock by the time the app was away.
    engine.handleEvent({.type = platform::Event::Type::FocusGained});
    EXPECT_FALSE(engine.getAudio().isOutputMuted());
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

// The storage and the preferences become durable for an app that the platform may end in the background, also when a listener of the event failed.
TEST_F(EngineTest, MakesSavedDataDurableInTheBackgroundEvenWhenAListenerFails) {
    test::EngineFixture fixture;
    fixture.runLua("require('haylen.events').on('appBackground', function() require('haylen.preferences').set('progress', 3) error('the save broke') end)");
    const int persisted = fixture.host().getPersistCount();
    fixture.engine().handleEvent({.type = platform::Event::Type::Suspended});
    EXPECT_EQ(fixture.host().getPersistCount(), persisted + 2);
    EXPECT_NE(fixture.engine().getStorage().readText("preferences.json").find("\"progress\": 3"), std::string::npos);
    ASSERT_NE(fixture.engine().getError(), nullptr);
    EXPECT_NE(std::string_view(fixture.engine().getError()->what()).find("the save broke"), std::string_view::npos);
}

TEST_F(EngineTest, PausesTheGame) {
    std::vector<std::string> log;
    test::EngineFixture fixture;
    Engine& engine = fixture.engine();
    const ScopedConnection paused(engine.pausedChanged.connect([&](bool value) { log.push_back(value ? "paused" : "resumed"); }));
    engine.getEvents().on(LifecycleEvent::kPaused, [&](EventBus::Event&) { log.emplace_back("event paused"); });
    engine.getEvents().on(LifecycleEvent::kUnpaused, [&](EventBus::Event&) { log.emplace_back("event unpaused"); });
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

TEST_F(EngineTest, ReportsLowMemoryToTheApp) {
    test::EngineFixture fixture({{"content/kept.json", "{}"}, {"content/dropped.json", "[]"}});
    assets::Manager& assets = fixture.engine().getAssets();
    const std::shared_ptr<void> kept = assets.load("json", "kept.json");
    (void)assets.load("json", "dropped.json");
    EXPECT_EQ(assets.getCachedCount(), 1U);

    std::vector<platform::Event::MemoryLevel> warnings;
    const ScopedConnection lowMemory(fixture.engine().lowMemory.connect([&](platform::Event::MemoryLevel level) { warnings.push_back(level); }));
    // clang-format off
    fixture.runLua(R"(
        levels = {}
        require('haylen.scene').push({event = function(_, event) if event.type == 'lowMemory' then levels[#levels + 1] = 'scene ' .. event.level end end})
        require('haylen.events').on('appLowMemory', function(data) levels[#levels + 1] = 'app ' .. data.level end)
    )");
    // clang-format on

    fixture.frames(1);
    fixture.engine().handleEvent({.type = platform::Event::Type::LowMemory, .memoryLevel = platform::Event::MemoryLevel::Warning});
    fixture.engine().handleEvent({.type = platform::Event::Type::LowMemory, .memoryLevel = platform::Event::MemoryLevel::Critical});
    EXPECT_EQ(warnings, (std::vector<platform::Event::MemoryLevel>{platform::Event::MemoryLevel::Warning, platform::Event::MemoryLevel::Critical}));
    EXPECT_EQ(fixture.lua("return table.concat(levels, ', ')"), "app warning, scene warning, app critical, scene critical");
    EXPECT_EQ(assets.releaseUnused(), 0U) << "The warning already pruned the entries of released assets.";
}

TEST_F(EngineTest, PublishesTheLinksThatOpenTheApp) {
    // The link that launched the process waits until the main script of the app listens.
    platform::LinkRelay::receive("island://open?map=harbor");
    test::EngineFixture fixture({{"source/main.lua", "links = {} require('haylen.events').on('linkOpened', function(link) links[#links + 1] = link.url end)"}});
    fixture.frames(1);
    platform::LinkRelay::receive("island://open?map=cave");
    platform::LinkRelay::receive("island://open?map=peak");
    fixture.frames(1);
    EXPECT_EQ(fixture.lua("return table.concat(links, ', ')"), "island://open?map=harbor, island://open?map=cave, island://open?map=peak");
}

// The schemes of the links belong to the app that the process installed, so a link ends a launched app and waits for the app that launched it.
TEST_F(EngineTest, EndsALaunchedAppForTheLinksOfTheAppThatLaunchedIt) {
    test::EngineFixture fixture({{"source/main.lua", "links = 0 require('haylen.events').on('linkOpened', function() links = links + 1 end)"}});
    fixture.host().setLaunched(true);
    fixture.frames(1);
    EXPECT_TRUE(fixture.engine().isRunning());

    platform::LinkRelay::receive("island://open?map=cave");
    fixture.frames(1);
    EXPECT_FALSE(fixture.engine().isRunning());
    EXPECT_EQ(fixture.lua("return links"), "0");
    EXPECT_EQ(platform::LinkRelay::take(), std::vector<std::string>{"island://open?map=cave"});
}

// No frame is complete before the engine rendered one, so a splash screen never ends over a window that holds nothing yet.
TEST_F(EngineTest, CompletesNoFrameBeforeItRenderedOne) {
    test::EngineFixture fixture;
    EXPECT_FALSE(fixture.engine().isFrameComplete());
    fixture.frames(1);
    EXPECT_TRUE(fixture.engine().isFrameComplete());
}

TEST_F(EngineTest, ReportsErrorsAndKeepsRendering) {
    std::vector<std::string> log;
    auto scene = std::make_shared<test::RecordingScene>("main", log);
    // clang-format off
    test::EngineFixture fixture({}, std::make_unique<test::TestApplication>([&](Engine& engine) {
        engine.getScenes().push(scene);
    }));
    // clang-format on

    std::string reported;
    const ScopedConnection connection(fixture.engine().errorRaised.connect([&](const core::AppError& error) { reported = error.what(); }));
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

// A Lua file that does not compile stops the app with the code "syntax", whether the engine loads it, a "require" of the app does or the app raises the message of that failure again.
TEST_F(EngineTest, NamesTheKindAndThePhaseOfErrors) {
    {
        test::EngineFixture fixture({{"source/main.lua", "local broken = "}});
        EXPECT_EQ(fixture.engine().getErrorRecord().at("code"), "syntax");
        EXPECT_EQ(fixture.engine().getErrorRecord().at("phase"), "startup");
    }
    {
        test::EngineFixture fixture({{"source/main.lua", "require('haylen.scene').push({update = function() require('broken') end})"}, {"source/broken.lua", "return {"}});
        fixture.frames(2);
        EXPECT_EQ(fixture.engine().getErrorRecord().at("code"), "syntax");
        EXPECT_EQ(fixture.engine().getErrorRecord().at("phase"), "frame");
        EXPECT_EQ(fixture.engine().getErrorRecord().at("file"), "source/broken.lua");
    }
    test::EngineFixture fixture({{"source/main.lua", "local ok, message = pcall(require, 'broken') error(message, 0)"}, {"source/broken.lua", "return {"}});
    EXPECT_EQ(fixture.engine().getErrorRecord().at("code"), "syntax");
}

TEST_F(EngineTest, NamesThePhaseOfErrorsAfterAStartThatFailed) {
    test::EngineFixture fixture({{"source/main.lua", "require('haylen.timer').after(0, function() error('later', 0) end)\nerror('first', 0)"}});
    EXPECT_EQ(fixture.engine().getErrorRecord().at("phase"), "startup");

    fixture.engine().recover();
    fixture.frames(3);
    ASSERT_NE(fixture.engine().getError(), nullptr);
    EXPECT_STREQ(fixture.engine().getError()->what(), "later");
    EXPECT_EQ(fixture.engine().getErrorRecord().at("phase"), "frame");
}

// A job that failed never runs again, so its failure restarts the app in development instead of resuming it without the work of the job.
TEST_F(EngineTest, RestartsInsteadOfResumingAfterAJobFailed) {
    test::EngineFixture fixture;
    fixture.engine().getJobs().post([] { throw std::runtime_error("The map could not be generated."); });
    for (int frame = 0; frame < 600 && fixture.engine().getError() == nullptr; ++frame) {
        fixture.frames(1);
    }

    ASSERT_NE(fixture.engine().getError(), nullptr);
    EXPECT_STREQ(fixture.engine().getError()->what(), "The map could not be generated.");
    EXPECT_FALSE(fixture.engine().isErrorResumable());
}

TEST_F(EngineTest, HandsTheErrorThatStoppedTheAppToTheHost) {
    test::EngineFixture fixture({{"source/main.lua", "local function explode()\n    error('the island sank')\nend\nexplode()"}});
    ASSERT_EQ(fixture.host().getErrorReports().size(), 1U);
    const Json& report = fixture.host().getErrorReports().front();
    EXPECT_EQ(report.at("message"), "the island sank");
    EXPECT_EQ(report.at("file"), "source/main.lua");
    EXPECT_EQ(report.at("line"), 2);
    EXPECT_NE(report.at("traceback").get<std::string>().find("source/main.lua:2"), std::string::npos);
    ASSERT_FALSE(report.at("frames").empty());
    EXPECT_EQ(report.at("frames").at(1), (Json{{"source", "source/main.lua"}, {"line", 2}, {"function", "local 'explode'"}, {"kind", "script"}}));
    EXPECT_EQ(report.at("code"), "runtime");
    EXPECT_EQ(report.at("phase"), "startup");
    EXPECT_EQ(report, fixture.engine().getErrorRecord());

    // Only the error that the error screen shows reaches the host.
    fixture.engine().reportError("a later failure");
    EXPECT_EQ(fixture.host().getErrorReports().size(), 1U);
}

TEST_F(EngineTest, TurnsStartupFailuresIntoErrors) {
    {
        test::EngineFixture failing({}, std::make_unique<test::TestApplication>([](Engine&) { throw std::runtime_error("cannot start"); }));
        ASSERT_NE(failing.engine().getError(), nullptr);
        EXPECT_STREQ(failing.engine().getError()->what(), "cannot start");
    }

    test::EngineFixture broken({{"source/main.lua", "this is not lua"}});
    ASSERT_NE(broken.engine().getError(), nullptr);
    EXPECT_NE(std::string_view(broken.engine().getError()->what()).find("source/main.lua"), std::string::npos);
}

TEST_F(EngineTest, ReportsFailuresFromAsynchronousCallbacksAndRendering) {
    test::EngineFixture fixture;
    fixture.engine().getJobs().postToFrame([] { throw std::runtime_error("late failure"); });
    fixture.frames(1);
    ASSERT_NE(fixture.engine().getError(), nullptr);
    EXPECT_STREQ(fixture.engine().getError()->what(), "late failure");
}

TEST_F(EngineTest, ReportsFailuresOfPluginsInEveryHookOfAFrame) {
    for (const auto& [hook, message] : std::vector<std::pair<FailingFramePlugin::Hook, std::string_view>>{{FailingFramePlugin::Hook::Event, "event failed"}, {FailingFramePlugin::Hook::Overlay, "overlay failed"}, {FailingFramePlugin::Hook::EndFrame, "end of frame failed"}}) {
        test::EngineFixture fixture({}, std::make_unique<test::TestApplication>([failing = hook](Engine& engine) { engine.addPlugin(std::make_unique<FailingFramePlugin>(failing)); }));
        fixture.engine().handleEvent({.type = platform::Event::Type::KeyDown, .key = input::Key::A});
        fixture.frames(1);
        ASSERT_NE(fixture.engine().getError(), nullptr) << message;
        EXPECT_EQ(std::string_view(fixture.engine().getError()->what()), message);
    }
}

TEST_F(EngineTest, StartsPluginsAddedBeforeItStartsOnce) {
    test::TemporaryDirectory directory;
    platform::HeadlessHost host(directory.getPath());
    std::optional<Engine::Phase> startPhase;
    Engine engine(host, std::make_shared<io::MemoryPackage>("app"), AppConfig{}, std::make_unique<test::TestApplication>([&startPhase](Engine& running) { startPhase = running.getPhase(); }));
    const CountingPlugin& plugin = engine.addPlugin(std::make_unique<CountingPlugin>());
    EXPECT_EQ(plugin.started, 0);

    // The start of the app runs as a lifecycle step, and a second start changes nothing.
    engine.start();
    engine.start();
    EXPECT_EQ(plugin.started, 1);
    EXPECT_EQ(startPhase, Engine::Phase::Lifecycle);
    EXPECT_EQ(engine.getPhase(), Engine::Phase::Frame);
    engine.stop();
    EXPECT_EQ(plugin.stopped, 1);
}

TEST_F(EngineTest, ReportsFailuresOfListenersWhenNativeUiCoversTheApp) {
    test::EngineFixture fixture;
    fixture.runLua("require('haylen.events').on('appInactive', function() error('the cover broke', 0) end)");
    fixture.host().getNativeViews().coverApp();
    fixture.frames(1);
    ASSERT_NE(fixture.engine().getError(), nullptr);
    EXPECT_STREQ(fixture.engine().getError()->what(), "the cover broke");
    EXPECT_EQ(fixture.engine().getAppState(), Engine::AppState::Inactive);
}

TEST_F(EngineTest, StopsOnlyThePluginsThatStarted) {
    int stops = 0;
    test::EngineFixture fixture({}, std::make_unique<test::TestApplication>([&stops](Engine& engine) { engine.addPlugin(std::make_unique<FailingStartPlugin>(stops)); }));
    ASSERT_NE(fixture.engine().getError(), nullptr);
    EXPECT_STREQ(fixture.engine().getError()->what(), "start failed");
    fixture.engine().stop();
    EXPECT_EQ(stops, 0);
}

TEST_F(EngineTest, ShowsErrorsOfAsyncTasksWithTheirStack) {
    test::EngineFixture fixture({{"source/scenes/loader.lua", "return function()\n    require('async').sleep(1):await()\n    error('the island failed to load')\nend"}});
    fixture.runLua("require('async').spawn(require('scenes.loader'))");
    EXPECT_EQ(fixture.engine().getError(), nullptr);
    ASSERT_TRUE(fixture.frameUntil([&] { return fixture.engine().getError() != nullptr; }));
    const core::AppError& error = *fixture.engine().getError();
    EXPECT_EQ(error.getMessage(), "the island failed to load");
    EXPECT_EQ(error.getFile(), "source/scenes/loader.lua");
    EXPECT_EQ(error.getLine(), 3);

    // The task shows its own frames only, without the native entry of Varn that runs it, and the task function, which native code calls, shows where it is defined.
    ASSERT_EQ(error.getFrames().size(), 2U);
    EXPECT_EQ(error.getFrames()[0].source, "[C]");
    EXPECT_EQ(error.getFrames()[0].function, "global 'error'");
    EXPECT_EQ(error.getFrames()[0].kind, core::AppError::Frame::Kind::Native);
    EXPECT_EQ(error.getFrames()[1].source, "source/scenes/loader.lua");
    EXPECT_EQ(error.getFrames()[1].line, 3);
    EXPECT_EQ(error.getFrames()[1].function, "function <source/scenes/loader.lua:1>");
    EXPECT_EQ(error.getFrames()[1].kind, core::AppError::Frame::Kind::Script);
    ASSERT_FALSE(fixture.host().getErrorReports().empty());
    EXPECT_EQ(fixture.host().getErrorReports().back(), fixture.engine().getErrorRecord());
    EXPECT_NE(fixture.lua("require('async').spawn(3)").find("bad argument #1 to 'spawn' (function expected, got number)"), std::string::npos);
}

TEST_F(EngineTest, BoundsThePollOfTheEventLoopByTheFrameTime) {
    test::EngineFixture fixture;
    // clang-format off
    fixture.runLua(R"(
        local async = require('async')
        steps = 0
        for task = 1, 2000 do
            async.spawn(function()
                while true do
                    steps = steps + 1
                    async.yield():await()
                end
            end)
        end
    )");
    // clang-format on

    // Tasks that always hand the loop back would keep an unbounded poll running for ever, so each frame takes what its budget allows and leaves the rest ready.
    const auto started = std::chrono::steady_clock::now();
    fixture.frames(1);
    EXPECT_LT(std::chrono::steady_clock::now() - started, std::chrono::seconds(1));
    const Engine::EventLoopPoll& polled = fixture.engine().getEventLoopPoll();
    EXPECT_EQ(polled.budget, std::chrono::nanoseconds(4166666));
    EXPECT_TRUE(polled.ran);
    EXPECT_TRUE(polled.pending);
    EXPECT_EQ(polled.idleMilliseconds, 0);
    const std::string first = fixture.lua("return steps");
    EXPECT_TRUE(fixture.frameUntil([&] { return fixture.lua("return steps") != first; }));
    EXPECT_EQ(fixture.lua("local loop = require('haylen.debug').stats().eventLoop return string.format('%.3f %s %s %d', loop.budget, loop.ran, loop.pending, loop.idle)"), "4.167 true true 0");

    // The budget is a quarter of the frame time, never less than 1 millisecond and never more than 8.
    fixture.frames(1, 1.0);
    EXPECT_EQ(fixture.engine().getEventLoopPoll().budget, std::chrono::milliseconds(8));
    fixture.frames(1, 0.001);
    EXPECT_EQ(fixture.engine().getEventLoopPoll().budget, std::chrono::milliseconds(1));
    EXPECT_EQ(fixture.engine().getError(), nullptr);
}

TEST_F(EngineTest, KeepsRunningWhileEveryRejectionIsObservedOrHandled) {
    test::EngineFixture fixture;
    // clang-format off
    fixture.runLua(R"(
        local async = require('async')
        local platform = require('haylen.platform')
        local function failing(message)
            return async.promise(function()
                async.sleep(1):await()
                error(message, 0)
            end)
        end
        caught = failing('A failure that does not matter.'):catch(function(message) return 'caught ' .. message end)
        abandoned = platform.call('store.slow')
        abandoned:cancel()
        async.spawn(function()
            local results = async.allSettled({failing('First.'), failing('Second.')}):await()
            settled = tostring(results[1].ok) .. ' ' .. results[2].error
            recovered = caught:await()
        end)
    )");
    // clang-format on

    ASSERT_TRUE(fixture.frameUntil([&] { return fixture.lua("return tostring(recovered ~= nil)") == "true"; }));
    fixture.frames(2);
    EXPECT_EQ(fixture.lua("return settled .. ' | ' .. recovered"), "false Second. | caught A failure that does not matter.");
    EXPECT_EQ(fixture.engine().getError(), nullptr);
}

TEST_F(EngineTest, StopsTheAppWhenARejectionOfNativeCodeIsNotObserved) {
    test::EngineFixture fixture({{"source/main.lua", "require('haylen.jobs').spawn(function() error('The job that nobody awaits failed.', 0) end)"}});
    ASSERT_TRUE(fixture.frameUntil([&] { return fixture.engine().getError() != nullptr; }));
    EXPECT_TRUE(fixture.engine().getError()->getMessage().starts_with("A promise rejected, and nothing awaited or caught it. The job that nobody awaits failed.")) << fixture.engine().getError()->getMessage();
}

TEST_F(EngineTest, ReportsRenderSubmissionFailures) {
    test::EngineFixture fixture;
    fixture.host().resize({20000.0F, 20000.0F});
    fixture.engine().getScenes().push(std::make_shared<test::DrawingScene>([](Engine& engine) { engine.getRenderer2D().beginWorld(graphics2d::Camera{}, {.ambientLight = math::Color::black()}); }));
    fixture.frames(2);
    ASSERT_NE(fixture.engine().getError(), nullptr);
    EXPECT_NE(std::string_view(fixture.engine().getError()->what()).find("Texture dimensions exceed the device limit"), std::string::npos);

    fixture.host().resize({1024.0F, 768.0F});
    fixture.frames(1);
    EXPECT_GT(fixture.engine().getRenderer2D().getStats().sprites, 0U);
}

TEST_F(EngineTest, AllowsOnlyOneEngineAtATime) {
    test::EngineFixture running;
    EXPECT_THROW(test::EngineFixture second, std::logic_error);
    running.frames(1);
}

TEST_F(EngineTest, QuitStopsFramesAndAsksTheHost) {
    test::EngineFixture fixture;
    EXPECT_TRUE(fixture.engine().isRunning());
    fixture.engine().quit();
    EXPECT_FALSE(fixture.engine().isRunning());
    EXPECT_TRUE(fixture.host().isQuitRequested());

    const std::uint64_t before = fixture.engine().getClock().getFrameIndex();
    fixture.frames(3);
    EXPECT_EQ(fixture.engine().getClock().getFrameIndex(), before);
}

TEST_F(EngineTest, KeepsTheExitStatusOfTheQuit) {
    test::EngineFixture fixture;
    EXPECT_EQ(fixture.engine().getExitStatus(), 0);
    fixture.engine().quit(42);
    EXPECT_FALSE(fixture.engine().isRunning());
    EXPECT_EQ(fixture.engine().getExitStatus(), 42);
}

TEST_F(EngineTest, ExposesServicesAndPlatformName) {
    test::EngineFixture fixture;
    Engine& engine = fixture.engine();
    EXPECT_EQ(engine.getPlatformName(), "headless");
    EXPECT_EQ(engine.getConfig().identifier, "com.haylen.tests");
    EXPECT_EQ(&engine.getWindow(), &fixture.host());
    EXPECT_EQ(engine.getPackage().getName(), "test");
    EXPECT_NE(engine.getLuaState(), nullptr);
    EXPECT_NE(engine.getDefaultFont(), nullptr);
    EXPECT_NE(engine.getPlugins().find("core"), nullptr);
    EXPECT_NE(engine.getPlugins().find("graphics2d"), nullptr);
    EXPECT_NE(engine.getPlugins().find("physics2d"), nullptr);
    EXPECT_NE(engine.getPlugins().find("assets"), nullptr);
}

TEST_F(EngineTest, ConstructorRejectsMissingParts) {
    test::TemporaryDirectory directory;
    platform::HeadlessHost host(directory.getPath());
    EXPECT_THROW(Engine(host, nullptr, AppConfig{}, std::make_unique<lua::Application>()), std::invalid_argument);
    EXPECT_THROW(Engine(host, std::make_shared<io::MemoryPackage>("empty"), AppConfig{}, nullptr), std::invalid_argument);
}

} // namespace haylen::core
