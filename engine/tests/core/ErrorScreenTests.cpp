#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "core/ErrorScreen.hpp"
#include "haylen/2d/graphics/Renderer.hpp"
#include "haylen/core/Log.hpp"
#include "haylen/core/SceneManager.hpp"
#include "haylen/core/Version.hpp"
#include "haylen/graphics/Viewport.hpp"
#include "haylen/input/GamepadState.hpp"
#include "haylen/lua/Error.hpp"
#include "haylen/platform/Event.hpp"
#include "support/DrawingScene.hpp"
#include "support/EngineFixture.hpp"

namespace haylen::core {

class ErrorScreenTest : public ::testing::Test {
  protected:
    static constexpr std::string_view kBattle = "local function strike(enemy)\n\treturn enemy.health\nend\n\nreturn function()\n\tlocal damage = strike(nil)\n\treturn damage\nend\n";

    // Starts an app whose scene fails on its first update, in the `strike` function on line 2 of `source/scenes/battle.lua`.
    [[nodiscard]] static test::EngineFixture startFailingApp() {
        return test::EngineFixture({{"source/scenes/battle.lua", std::string(kBattle)}, {"source/main.lua", "require('haylen.scene').push({update = require('scenes.battle')})"}});
    }

    static void press(test::EngineFixture& fixture, input::Key key) {
        platform::Event event;
        event.type = platform::Event::Type::KeyDown;
        event.key = key;
        fixture.engine().handleEvent(event);
    }

    // Sends a mouse or touch event at a point in design units, which the platform reports in framebuffer pixels.
    static void point(test::EngineFixture& fixture, ErrorScreen& screen, platform::Event::Type type, math::Vec2 design, input::MouseButton button = input::MouseButton::Left) {
        platform::Event event;
        event.type = type;
        event.mouseButton = button;
        event.position = fixture.engine().getViewport().toFramebuffer(design);
        event.touchCount = 1;
        event.touches[0] = {.id = 1, .position = event.position, .changed = true};
        screen.handleEvent(event);
    }
};

TEST_F(ErrorScreenTest, ShowsTheLinesAroundTheErrorLineOfAPackageFile) {
    test::EngineFixture fixture = startFailingApp();

    const core::ErrorScreen middle(fixture.engine(), lua::Error("source/scenes/battle.lua:6: boom"));
    const std::vector<core::ErrorScreen::SourceLine>& lines = middle.getExcerpt();
    ASSERT_EQ(lines.size(), 6U);
    EXPECT_EQ(lines.front().number, 3);
    EXPECT_EQ(lines.back().number, 8);
    EXPECT_EQ(lines[3].number, 6);
    EXPECT_EQ(lines[3].text, "    local damage = strike(nil)");
    EXPECT_EQ(lines[1].text, "");

    const core::ErrorScreen first(fixture.engine(), lua::Error("source/scenes/battle.lua:1: boom"));
    ASSERT_EQ(first.getExcerpt().size(), 4U);
    EXPECT_EQ(first.getExcerpt().front().text, "local function strike(enemy)");

    EXPECT_TRUE(core::ErrorScreen(fixture.engine(), lua::Error("source/scenes/battle.lua:40: past the end")).getExcerpt().empty());
    EXPECT_TRUE(core::ErrorScreen(fixture.engine(), lua::Error("source/scenes/missing.lua:3: boom")).getExcerpt().empty());
    EXPECT_TRUE(core::ErrorScreen(fixture.engine(), lua::Error("The app could not be loaded.")).getExcerpt().empty());
}

TEST_F(ErrorScreenTest, ReportsTheWholeErrorAsPlainText) {
    std::vector<std::string> logged;
    const std::uint64_t listener = core::Log::addListener([&logged](core::Log::Level, std::string_view line) { logged.emplace_back(line); });
    test::EngineFixture fixture = startFailingApp();
    fixture.frames(1);
    core::Log::removeListener(listener);

    ASSERT_NE(fixture.engine().getError(), nullptr);
    const core::ErrorScreen screen(fixture.engine(), *fixture.engine().getError());
    const std::string& report = screen.getReport();
    EXPECT_TRUE(report.starts_with("The app stopped with an error\nTest App 1.0.0 · headless · Haylen " + std::string(core::Version::kString) + "\n\nattempt to index")) << report;
    EXPECT_NE(report.find("\nFile \"source/scenes/battle.lua\", line 2\n"), std::string::npos) << report;
    EXPECT_NE(report.find("\n> 2      return enemy.health\n"), std::string::npos) << report;
    EXPECT_NE(report.find("\n  5  return function()\n"), std::string::npos) << report;
    EXPECT_TRUE(report.ends_with("\n\nStack\nsource/scenes/battle.lua:2  upvalue 'strike'\nsource/scenes/battle.lua:6  function <source/scenes/battle.lua:5>")) << report;
    EXPECT_EQ(report.find('\t'), std::string::npos);

    // The log receives the same report once, as the error happens.
    EXPECT_EQ(std::count_if(logged.begin(), logged.end(), [&report](const std::string& line) { return line.find(report) != std::string::npos; }), 1);
}

TEST_F(ErrorScreenTest, CopiesTheReportAndRestartsTheAppFromTheKeyboard) {
    test::EngineFixture fixture = startFailingApp();
    fixture.frames(2);
    ASSERT_NE(fixture.engine().getError(), nullptr);
    const std::string report = core::ErrorScreen(fixture.engine(), *fixture.engine().getError()).getReport();

    press(fixture, input::Key::C);
    EXPECT_EQ(fixture.host().getClipboard(), report);
    EXPECT_FALSE(fixture.engine().isRestartRequested());

    press(fixture, input::Key::R);
    EXPECT_TRUE(fixture.engine().isRestartRequested());
}

TEST_F(ErrorScreenTest, CopiesAndRestartsWithAGamepadOrATvRemote) {
    test::EngineFixture fixture = startFailingApp();
    fixture.frames(2);
    ASSERT_NE(fixture.engine().getError(), nullptr);
    const std::string report = core::ErrorScreen(fixture.engine(), *fixture.engine().getError()).getReport();

    // A button counts as pressed in the frame it goes down, so each press is one frame down and one frame up.
    // clang-format off
    const auto tap = [&fixture](input::GamepadButton button) {
        input::GamepadState state{.connected = true, .name = "Remote"};
        state.buttons[static_cast<std::size_t>(button)] = true;
        fixture.host().setGamepad(0, state);
        fixture.frames(1);
        fixture.host().setGamepad(0, {.connected = true, .name = "Remote"});
        fixture.frames(1);
    };
    // clang-format on

    // The focus starts on Restart, and the directional pad moves it to Copy, which the south button presses.
    tap(input::GamepadButton::DpadLeft);
    tap(input::GamepadButton::South);
    EXPECT_EQ(fixture.host().getClipboard(), report);
    EXPECT_FALSE(fixture.engine().isRestartRequested());

    tap(input::GamepadButton::DpadRight);
    tap(input::GamepadButton::South);
    EXPECT_TRUE(fixture.engine().isRestartRequested());
}

TEST_F(ErrorScreenTest, FollowsTheArrowKeysAndEnterOfATvRemote) {
    test::EngineFixture fixture = startFailingApp();
    fixture.frames(2);
    ASSERT_NE(fixture.engine().getError(), nullptr);

    // A keyboard with a pointer leaves the focus alone, since it shows no focus there.
    press(fixture, input::Key::Left);
    press(fixture, input::Key::Enter);
    EXPECT_TRUE(fixture.host().getClipboard().empty());
    EXPECT_FALSE(fixture.engine().isRestartRequested());

    // A TV has no pointer, and its remote sends arrow keys and Enter.
    fixture.host().setPointerDevice(false);
    press(fixture, input::Key::Left);
    press(fixture, input::Key::Enter);
    EXPECT_FALSE(fixture.host().getClipboard().empty());
    EXPECT_FALSE(fixture.engine().isRestartRequested());
    press(fixture, input::Key::Right);
    press(fixture, input::Key::Enter);
    EXPECT_TRUE(fixture.engine().isRestartRequested());
}

TEST_F(ErrorScreenTest, CopiesAndRestartsWithAClickOrATap) {
    test::EngineFixture fixture({{"source/scenes/battle.lua", std::string(kBattle)}, {"source/main.lua", ""}});
    ErrorScreen screen(fixture.engine(), lua::Error("source/scenes/battle.lua:6: attempt to call a nil value"));
    fixture.engine().getScenes().push(std::make_shared<test::DrawingScene>([&screen](Engine&) { screen.render(); }));
    fixture.frames(2);

    // Only the left button clicks, and the action runs as the button goes down.
    point(fixture, screen, platform::Event::Type::MouseDown, screen.getCopyButton().getCenter(), input::MouseButton::Right);
    EXPECT_TRUE(fixture.host().getClipboard().empty());
    point(fixture, screen, platform::Event::Type::MouseDown, screen.getCopyButton().getCenter());
    point(fixture, screen, platform::Event::Type::MouseUp, screen.getCopyButton().getCenter());
    EXPECT_EQ(fixture.host().getClipboard(), screen.getReport());
    EXPECT_FALSE(fixture.engine().isRestartRequested());

    point(fixture, screen, platform::Event::Type::TouchBegan, screen.getRestartButton().getCenter());
    point(fixture, screen, platform::Event::Type::TouchEnded, screen.getRestartButton().getCenter());
    EXPECT_TRUE(fixture.engine().isRestartRequested());
}

TEST_F(ErrorScreenTest, GoesBackToARecoverableAppAfterItsListenersRan) {
    // clang-format off
    test::EngineFixture fixture({{"source/main.lua", R"(
        local events = require('haylen.events')
        local haylen = require('haylen')
        local scene = require('haylen.scene')
        log = {}
        haylen.setRecoverable(true)
        events.on('appError', function(error) log[#log + 1] = 'error ' .. error.message .. ' ' .. tostring(scene.size()) end)
        events.on('appRecovered', function(error)
            log[#log + 1] = 'recovered ' .. error.message
            scene.clear()
            scene.push({update = function() log[#log + 1] = 'menu' end})
        end)
        scene.push({update = function() error('broken scene', 0) end})
    )"}});
    // clang-format on
    fixture.frames(2);
    ASSERT_NE(fixture.engine().getError(), nullptr);
    EXPECT_TRUE(fixture.engine().isBackCaptured());
    EXPECT_EQ(fixture.lua("return table.concat(log, ', ')"), "error broken scene 1");

    // The listeners of appRecovered run before the app updates again, so the failing scene never runs once more.
    press(fixture, input::Key::Escape);
    EXPECT_NE(fixture.engine().getError(), nullptr);
    fixture.frames(1);
    EXPECT_EQ(fixture.engine().getError(), nullptr);
    EXPECT_EQ(fixture.lua("return table.concat(log, ', ')"), "error broken scene 1, recovered broken scene, menu");
    EXPECT_EQ(fixture.lua("return tostring(require('haylen').recoverable())"), "true");
}

TEST_F(ErrorScreenTest, OffersToGoBackOnlyInARecoverableApp) {
    test::EngineFixture fixture({{"source/scenes/battle.lua", std::string(kBattle)}, {"source/main.lua", ""}});
    ErrorScreen plain(fixture.engine(), lua::Error("source/scenes/battle.lua:6: boom"));
    fixture.engine().setRecoverable(true);
    ErrorScreen recoverable(fixture.engine(), lua::Error("source/scenes/battle.lua:6: boom"));
    EXPECT_EQ(plain.getFocusedAction(), ErrorScreen::Action::Restart);
    EXPECT_EQ(recoverable.getFocusedAction(), ErrorScreen::Action::Back);

    // clang-format off
    fixture.engine().getScenes().push(std::make_shared<test::DrawingScene>([&](Engine&) {
        plain.render();
        recoverable.render();
    }));
    // clang-format on
    fixture.frames(2);
    EXPECT_GT(recoverable.getBackButton().getSize().x, 0.0F);

    // Recovery waits for an error that stops the app, and a press that goes back to an app without one changes nothing.
    point(fixture, recoverable, platform::Event::Type::MouseDown, recoverable.getBackButton().getCenter());
    fixture.frames(1);
    EXPECT_EQ(fixture.engine().getError(), nullptr);
    fixture.engine().setRecoverable(false);
    EXPECT_FALSE(fixture.engine().isRecoverable());
}

TEST_F(ErrorScreenTest, GoesBackWithTheEastButtonOfAGamepad) {
    test::EngineFixture fixture({{"source/main.lua", "require('haylen').setRecoverable(true) require('haylen.scene').push({update = function() error('broken', 0) end})"}});
    fixture.frames(2);
    ASSERT_NE(fixture.engine().getError(), nullptr);
    input::GamepadState state{.connected = true};
    state.buttons[static_cast<std::size_t>(input::GamepadButton::East)] = true;
    fixture.host().setGamepad(0, state);
    fixture.frames(1);
    fixture.host().setGamepad(0, {.connected = true});

    // The scene fails again once the app runs on, which shows the screen again.
    fixture.frames(1);
    EXPECT_NE(fixture.engine().getError(), nullptr);
    EXPECT_EQ(fixture.host().getErrorReports().size(), 2U);
}

TEST_F(ErrorScreenTest, ScrollsContentTallerThanTheScreen) {
    test::EngineFixture fixture({{"source/scenes/battle.lua", std::string(kBattle)}, {"source/main.lua", ""}});
    fixture.host().resize({480.0F, 320.0F});
    ErrorScreen screen(fixture.engine(), lua::Error("source/scenes/battle.lua:6: attempt to call a nil value"));
    fixture.engine().getScenes().push(std::make_shared<test::DrawingScene>([&screen](Engine&) { screen.render(); }));
    fixture.frames(2);
    ASSERT_GT(screen.getMaxScroll(), 0.0F);
    const math::Vec2 content = fixture.engine().getViewport().getSafeRect().getCenter();

    platform::Event wheel;
    wheel.type = platform::Event::Type::MouseScroll;
    wheel.scroll = {0.0F, -1.0F};
    screen.handleEvent(wheel);
    const float step = screen.getScroll();
    EXPECT_GT(step, 0.0F);
    wheel.scroll = {0.0F, 1.0F};
    screen.handleEvent(wheel);
    EXPECT_EQ(screen.getScroll(), 0.0F);

    // A drag moves the content with the pointer and stops at its end, and a press on a button starts no drag.
    point(fixture, screen, platform::Event::Type::MouseDown, content);
    point(fixture, screen, platform::Event::Type::MouseMove, content - math::Vec2{0.0F, 10.0F});
    EXPECT_NEAR(screen.getScroll(), 10.0F, 0.01F);
    point(fixture, screen, platform::Event::Type::MouseMove, content - math::Vec2{0.0F, 10000.0F});
    EXPECT_EQ(screen.getScroll(), screen.getMaxScroll());
    point(fixture, screen, platform::Event::Type::MouseUp, content);
    point(fixture, screen, platform::Event::Type::MouseMove, content);
    EXPECT_EQ(screen.getScroll(), screen.getMaxScroll());

    // The first finger drags, and a second finger that lands on a button meanwhile presses nothing.
    point(fixture, screen, platform::Event::Type::TouchBegan, content);
    platform::Event second;
    second.type = platform::Event::Type::TouchBegan;
    second.touchCount = 2;
    second.touches[0] = {.id = 1, .position = fixture.engine().getViewport().toFramebuffer(content)};
    second.touches[1] = {.id = 2, .position = fixture.engine().getViewport().toFramebuffer(screen.getRestartButton().getCenter()), .changed = true};
    screen.handleEvent(second);
    second.type = platform::Event::Type::TouchEnded;
    screen.handleEvent(second);
    EXPECT_FALSE(fixture.engine().isRestartRequested());
    point(fixture, screen, platform::Event::Type::TouchMoved, content + math::Vec2{0.0F, 10000.0F});
    EXPECT_EQ(screen.getScroll(), 0.0F);
    point(fixture, screen, platform::Event::Type::TouchEnded, content);
    point(fixture, screen, platform::Event::Type::MouseDown, screen.getCopyButton().getCenter());
    point(fixture, screen, platform::Event::Type::MouseMove, content - math::Vec2{0.0F, 10000.0F});
    EXPECT_EQ(screen.getScroll(), 0.0F);

    // The arrow keys move a line, and Home and End jump to the ends.
    for (const auto& [key, expected] : std::vector<std::pair<input::Key, float>>{{input::Key::Down, step}, {input::Key::End, screen.getMaxScroll()}, {input::Key::Up, screen.getMaxScroll() - step}, {input::Key::Home, 0.0F}}) {
        screen.handleEvent({.type = platform::Event::Type::KeyDown, .key = key});
        EXPECT_NEAR(screen.getScroll(), expected, 0.01F);
    }
}

TEST_F(ErrorScreenTest, DrawsAndScrollsInTheHeadlessEngine) {
    test::EngineFixture fixture = startFailingApp();
    fixture.frames(2);
    ASSERT_NE(fixture.engine().getError(), nullptr);
    EXPECT_GT(fixture.engine().getRenderer2D().getStats().sprites, 0U);

    // A small window makes the content taller than the screen, and every way of scrolling it keeps drawing.
    fixture.host().resize({480.0F, 320.0F});
    for (const input::Key key : {input::Key::Down, input::Key::PageDown, input::Key::End, input::Key::Up, input::Key::PageUp, input::Key::Home}) {
        press(fixture, key);
        fixture.frames(1);
    }

    platform::Event wheel;
    wheel.type = platform::Event::Type::MouseScroll;
    wheel.scroll = {0.0F, -3.0F};
    fixture.engine().handleEvent(wheel);

    platform::Event mouse;
    mouse.type = platform::Event::Type::MouseDown;
    mouse.position = {240.0F, 120.0F};
    fixture.engine().handleEvent(mouse);
    mouse.type = platform::Event::Type::MouseMove;
    mouse.position = {240.0F, 40.0F};
    fixture.engine().handleEvent(mouse);
    mouse.type = platform::Event::Type::MouseUp;
    fixture.engine().handleEvent(mouse);
    fixture.frames(1);

    platform::Event touch;
    touch.type = platform::Event::Type::TouchBegan;
    touch.touchCount = 1;
    touch.touches[0] = {.id = 1, .position = {240.0F, 100.0F}, .changed = true};
    fixture.engine().handleEvent(touch);
    touch.type = platform::Event::Type::TouchMoved;
    touch.touches[0].position = {240.0F, 20.0F};
    fixture.engine().handleEvent(touch);
    touch.type = platform::Event::Type::TouchEnded;
    fixture.engine().handleEvent(touch);
    fixture.frames(2);

    EXPECT_GT(fixture.engine().getRenderer2D().getStats().sprites, 0U);
    EXPECT_FALSE(fixture.engine().isRestartRequested());
    EXPECT_STREQ(fixture.engine().getError()->what(), "source/scenes/battle.lua:2: attempt to index a nil value (local 'enemy')");
}

} // namespace haylen::core
