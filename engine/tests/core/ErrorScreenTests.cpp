#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "core/ErrorScreen.hpp"
#include "haylen/2d/graphics/Renderer.hpp"
#include "haylen/core/Log.hpp"
#include "haylen/core/Version.hpp"
#include "haylen/lua/Error.hpp"
#include "haylen/platform/Event.hpp"
#include "support/EngineFixture.hpp"

namespace haylen::core {

class ErrorScreenTest : public ::testing::Test {
  protected:
    static constexpr std::string_view kBattle = "local function strike(enemy)\n\treturn enemy.health\nend\n\nreturn function()\n\tlocal damage = strike(nil)\n\treturn damage\nend\n";

    // Starts an app whose scene fails on its first update, in the strike function on line 2 of source/scenes/battle.lua.
    [[nodiscard]] static test::EngineFixture startFailingApp() {
        return test::EngineFixture({{"source/scenes/battle.lua", std::string(kBattle)}, {"source/main.lua", "require('haylen.scene').push({update = require('scenes.battle')})"}});
    }

    static void press(test::EngineFixture& fixture, input::Key key) {
        platform::Event event;
        event.type = platform::Event::Type::KeyDown;
        event.key = key;
        fixture.engine().handleEvent(event);
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
    EXPECT_NE(report.find("\nsource/scenes/battle.lua, line 2\n"), std::string::npos) << report;
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
