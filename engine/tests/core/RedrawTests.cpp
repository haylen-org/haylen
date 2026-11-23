#include <gtest/gtest.h>

#include <cstdint>
#include <map>
#include <memory>
#include <string>

#include "gpu/NullDevice.hpp"
#include "haylen/core/Engine.hpp"
#include "haylen/core/EventBus.hpp"
#include "haylen/core/FrameClock.hpp"
#include "haylen/platform/Event.hpp"
#include "haylen/platform/Window.hpp"
#include "haylen/plugins/Plugin.hpp"
#include "platform/headless/HeadlessHost.hpp"
#include "support/AllocationTracker.hpp"
#include "support/EngineFixture.hpp"

namespace haylen::core {

class RedrawTest : public ::testing::Test {
  protected:
    // Animates while the test says so, like a plugin whose picture moves by itself.
    class AnimatingPlugin final : public plugins::Plugin {
      public:
        [[nodiscard]] std::string_view getName() const noexcept override {
            return "animating";
        }
        [[nodiscard]] bool isAnimating() const override {
            return animating;
        }

        bool animating = false;
    };

    static constexpr std::uint64_t kSettle = Engine::kSettleFrames;
    static constexpr std::string_view kOnChange = R"({"name": "Test App", "identifier": "com.haylen.tests", "window": {"redraw": "onChange"}})";

    // A fixture of an app that redraws on change, which drew its first complete frames and the settle after them.
    [[nodiscard]] static std::unique_ptr<test::EngineFixture> makeIdle(std::map<std::string, std::string> files = {}) {
        files.emplace("haylen.json", std::string(kOnChange));
        auto fixture = std::make_unique<test::EngineFixture>(std::move(files));
        fixture->frames(10);
        return fixture;
    }

    [[nodiscard]] static std::uint64_t committed(test::EngineFixture& fixture) {
        return fixture.host().getGpu().getCommittedFrames();
    }

    // Runs frames and returns how many of them the device committed.
    [[nodiscard]] static std::uint64_t drawn(test::EngineFixture& fixture, int frames, double seconds = 1.0 / 60.0) {
        const std::uint64_t before = committed(fixture);
        fixture.frames(frames, seconds);
        return committed(fixture) - before;
    }
};

TEST_F(RedrawTest, DrawsEveryFrameByDefault) {
    test::EngineFixture fixture;
    fixture.frames(10);
    EXPECT_EQ(fixture.engine().getRedraw(), platform::Window::Redraw::Always);
    EXPECT_EQ(drawn(fixture, 30), 30U);
}

// A frame that draws nothing allocates nothing either, in the renderer, the text, the UI or the engine.
TEST_F(RedrawTest, IdleAppThatRedrawsOnChangeDrawsAndAllocatesNothing) {
    const std::unique_ptr<test::EngineFixture> fixture = makeIdle({{"source/main.lua", "local ui = require('haylen.ui') gui = ui.mount(ui.column{ui.label{text = 'Hello'}, ui.button{text = 'Press'}})"}});
    EXPECT_EQ(fixture->engine().getRedraw(), platform::Window::Redraw::OnChange);
    EXPECT_TRUE(fixture->engine().isFrameComplete());

    const test::AllocationTracker tracker;
    EXPECT_EQ(drawn(*fixture, 60), 0U);
    EXPECT_EQ(tracker.getTotal(), 0U);
    EXPECT_FALSE(fixture->engine().isDrawing());
}

// The frame that a change needs draws, and the settle of the frames after it draws too, so state that follows a frame behind shows.
TEST_F(RedrawTest, EventOfThePlatformDrawsTheFrameAndItsSettle) {
    const std::unique_ptr<test::EngineFixture> fixture = makeIdle();
    fixture->engine().handleEvent({.type = platform::Event::Type::MouseMove, .position = {10.0F, 10.0F}});
    EXPECT_EQ(drawn(*fixture, 10), 1U + kSettle);
}

TEST_F(RedrawTest, RequestFromCodeDrawsTheNextFrame) {
    const std::unique_ptr<test::EngineFixture> fixture = makeIdle();
    fixture->runLua("require('haylen.window').requestRedraw()");
    EXPECT_EQ(drawn(*fixture, 10), 1U + kSettle);
}

// The time of the frames that draw nothing still passes, so a timer fires on time and the frame it fires in draws.
TEST_F(RedrawTest, TimerFiresOnTimeAndDrawsItsFrame) {
    const std::unique_ptr<test::EngineFixture> fixture = makeIdle();
    fixture->runLua("haylen = require('haylen') started = haylen.elapsed() require('haylen.timer').after(0.5, function() fired = haylen.elapsed() end)");
    EXPECT_EQ(drawn(*fixture, 25), 0U);
    EXPECT_EQ(fixture->lua("return tostring(fired)"), "nil");

    EXPECT_EQ(drawn(*fixture, 20), 1U + kSettle);
    EXPECT_NEAR(std::stod(fixture->lua("return fired - started")), 0.5, 1.0 / 60.0 + 1.0e-6);
}

TEST_F(RedrawTest, TweenKeepsDrawingUntilItEnds) {
    const std::unique_ptr<test::EngineFixture> fixture = makeIdle();
    fixture->runLua("box = {x = 0} require('haylen.tween').to(box, 0.5, {x = 10})");
    EXPECT_EQ(drawn(*fixture, 20), 20U);
    EXPECT_NEAR(static_cast<double>(drawn(*fixture, 30)), 10.0 + static_cast<double>(kSettle), 1.0);
    EXPECT_EQ(fixture->lua("return box.x"), "10.0");
    EXPECT_EQ(drawn(*fixture, 20), 0U);
}

// After a long idle the next tween starts from its first step and the fixed steps owe nothing for the frames that drew nothing.
TEST_F(RedrawTest, FrameAfterALongIdleNeitherJumpsNorCatchesUp) {
    const std::unique_ptr<test::EngineFixture> fixture = makeIdle({{"source/main.lua", "steps = 0 require('haylen.scene').push({fixedUpdate = function() steps = steps + 1 end})"}});
    fixture->frames(10);
    EXPECT_EQ(drawn(*fixture, 600), 0U);

    fixture->runLua("steps = 0 box = {x = 0} require('haylen.tween').to(box, 1, {x = 60})");
    fixture->frames(1);
    EXPECT_EQ(fixture->lua("return steps"), "1");
    EXPECT_LT(std::stod(fixture->lua("return box.x")), 2.0);
}

// The task runs until its sleep as it spawns, and the frame it resumes in draws while the frames of its sleep draw nothing.
TEST_F(RedrawTest, WorkOfTheEventLoopDrawsItsFrames) {
    const std::unique_ptr<test::EngineFixture> fixture = makeIdle();
    const std::uint64_t before = committed(*fixture);
    fixture->runLua("done = false async = require('async') async.spawn(function() async.sleep(50):await() done = true end)");
    ASSERT_TRUE(fixture->frameUntil([&fixture] { return fixture->lua("return done") == "true"; }));
    fixture->frames(Engine::kSettleFrames);
    EXPECT_EQ(committed(*fixture) - before, 1U + kSettle);
    EXPECT_EQ(drawn(*fixture, 20), 0U);
}

TEST_F(RedrawTest, PluginThatAnimatesKeepsDrawing) {
    const std::unique_ptr<test::EngineFixture> fixture = makeIdle();
    AnimatingPlugin& plugin = fixture->engine().addPlugin(std::make_unique<AnimatingPlugin>());
    plugin.animating = true;
    EXPECT_EQ(drawn(*fixture, 20), 20U);

    plugin.animating = false;
    EXPECT_EQ(drawn(*fixture, 20), kSettle);
}

TEST_F(RedrawTest, ResizeDrawsTheNewSize) {
    const std::unique_ptr<test::EngineFixture> fixture = makeIdle();
    fixture->host().resize({800.0F, 600.0F});
    EXPECT_EQ(drawn(*fixture, 10), 1U + kSettle);
    EXPECT_EQ(fixture->host().getGpu().getSurfaceSize().width, 800);
}

TEST_F(RedrawTest, ListenerOfTheEventBusDrawsTheNextFrame) {
    const std::unique_ptr<test::EngineFixture> fixture = makeIdle();
    const Connection listener = fixture->engine().getEvents().on("changed", [](EventBus::Event&) {});
    fixture->engine().getEvents().emit("changed");
    EXPECT_EQ(drawn(*fixture, 10), 1U + kSettle);

    fixture->engine().getEvents().emit("unheard");
    EXPECT_EQ(drawn(*fixture, 10), 0U);
}

TEST_F(RedrawTest, ErrorScreenKeepsDrawing) {
    const std::unique_ptr<test::EngineFixture> fixture = makeIdle();
    fixture->runLua("require('haylen.timer').after(0, function() error('broken') end)");
    fixture->frames(2);
    ASSERT_NE(fixture->engine().getError(), nullptr);
    EXPECT_EQ(drawn(*fixture, 20), 20U);
}

TEST_F(RedrawTest, ParticlesThatLiveKeepDrawing) {
    const std::unique_ptr<test::EngineFixture> fixture = makeIdle();
    fixture->runLua("emitter = require('haylen.particles2d').newEmitter({texture = require('haylen.graphics').whiteTexture(), rate = 0, lifetime = {0.2, 0.2}}) emitter.emitting = false emitter:burst(10)");
    EXPECT_EQ(drawn(*fixture, 5), 5U);

    fixture->runLua("emitter:update(1)");
    EXPECT_EQ(drawn(*fixture, 10), kSettle);
}

TEST_F(RedrawTest, AwakeBodiesKeepDrawing) {
    const std::unique_ptr<test::EngineFixture> fixture = makeIdle();
    fixture->runLua("world = require('haylen.physics2d').newWorld({gravity = {0, 0}}) body = world:createBody({type = 'dynamic', x = 0, y = 0})");
    EXPECT_EQ(drawn(*fixture, 5), 5U);

    fixture->runLua("world:destroy()");
    EXPECT_EQ(drawn(*fixture, 10), kSettle);
}

TEST_F(RedrawTest, AlwaysDrawsAgainOnceTheAppAsksForIt) {
    const std::unique_ptr<test::EngineFixture> fixture = makeIdle();
    fixture->runLua("require('haylen.window').setRedraw('always')");
    EXPECT_EQ(fixture->lua("return require('haylen.window').redraw()"), "always");
    EXPECT_EQ(drawn(*fixture, 20), 20U);

    fixture->runLua("require('haylen.window').setRedraw('onChange')");
    EXPECT_EQ(fixture->lua("return require('haylen.window').redraw()"), "onChange");
    EXPECT_EQ(drawn(*fixture, 20), 1U + kSettle);
}

TEST_F(RedrawTest, RefusesAnUnknownRedraw) {
    const std::unique_ptr<test::EngineFixture> fixture = makeIdle();
    EXPECT_NE(fixture->lua("require('haylen.window').setRedraw('sometimes')").find("error:"), std::string::npos);
}

} // namespace haylen::core
