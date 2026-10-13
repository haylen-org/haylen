#include <gtest/gtest.h>

#include <cstddef>
#include <format>
#include <stdexcept>
#include <string>
#include <vector>

#include "haylen/core/Engine.hpp"
#include "haylen/core/FrameClock.hpp"
#include "haylen/debug/Console.hpp"
#include "haylen/platform/Event.hpp"
#include "haylen/plugins/DebugPlugin.hpp"
#include "platform/ConsoleRelay.hpp"
#include "support/EngineFixture.hpp"

namespace haylen::debug {

class ConsoleTest : public ::testing::Test {
  protected:
    void SetUp() override {
        // clang-format off
        gold = console.addCommand({.name = "gold", .help = "Adds gold.", .parameters = {{.kind = Console::Kind::Integer}, {.kind = Console::Kind::String, .optional = true}}, .handler = [this](const core::Json& arguments) {
            received = arguments;
            return std::format("Gold {}", arguments[0].get<std::int64_t>());
        }});
        // clang-format on
    }

    Console console;
    core::Connection gold;
    core::Json received;
};

TEST_F(ConsoleTest, RunsCommandsWithCheckedArguments) {
    const Console::Result added = console.run("  gold 50 \"the shop\"  ");
    EXPECT_TRUE(added.ok);
    EXPECT_EQ(added.output, "Gold 50");
    EXPECT_EQ(received, (core::Json{50, "the shop"}));
    EXPECT_TRUE(console.run("gold -3").ok);
    EXPECT_EQ(received, core::Json{-3});

    EXPECT_EQ(console.run("gold fifty").output, "The command \"gold\" needs an integer as its first argument, not \"fifty\". Type it as \"gold <integer> [word]\".");
    EXPECT_EQ(console.run("gold").output, "The command \"gold\" needs an integer as its first argument. Type it as \"gold <integer> [word]\".");
    EXPECT_EQ(console.run("gold 1 a b").output, "The command \"gold\" takes 2 arguments. Type it as \"gold <integer> [word]\".");
    EXPECT_EQ(console.run("silver 1").output, "There is no command \"silver\". Type \"help\" to list the commands.");
    EXPECT_EQ(console.run("gold \"1").output, "The line has a quote that is not closed.");
    EXPECT_FALSE(console.run("gold 1.5").ok);

    std::vector<std::string> said;
    // clang-format off
    const core::Connection say = console.addCommand({.name = "say", .help = "Says a text.", .parameters = {{.kind = Console::Kind::Number}, {.kind = Console::Kind::Text}}, .handler = [&said](const core::Json& arguments) {
        said.push_back(std::format("{} {}", arguments[0].get<double>(), arguments[1].get<std::string>()));
        return std::string();
    }});
    // clang-format on
    EXPECT_TRUE(console.run("say 1.5 Hello   \"there\" you").ok);
    EXPECT_EQ(said, std::vector<std::string>{"1.5 Hello   \"there\" you"});
    EXPECT_EQ(console.run("say inf words").output, "The command \"say\" needs a number as its first argument, not \"inf\". Type it as \"say <number> <text>\".");

    EXPECT_THROW((void)console.addCommand({.name = "two words", .help = {}, .parameters = {}, .handler = {}}), std::invalid_argument);
    EXPECT_THROW((void)console.addCommand({.name = "late", .help = {}, .parameters = {{.kind = Console::Kind::Integer, .optional = true}, {.kind = Console::Kind::Integer}}, .handler = {}}), std::invalid_argument);
    EXPECT_THROW((void)console.addCommand({.name = "early", .help = {}, .parameters = {{.kind = Console::Kind::Text}, {.kind = Console::Kind::Integer}}, .handler = {}}), std::invalid_argument);
}

TEST_F(ConsoleTest, FailsWithTheErrorOfAHandlerAndRemovesCommandsWithTheirConnection) {
    // clang-format off
    core::Connection broken = console.addCommand({.name = "broken", .help = "Fails.", .parameters = {}, .handler = [](const core::Json&) -> std::string {
        throw std::runtime_error("The shop is closed.");
    }});
    // clang-format on
    const Console::Result failed = console.run("broken");
    EXPECT_FALSE(failed.ok);
    EXPECT_EQ(failed.output, "The shop is closed.");
    EXPECT_EQ(console.getOutput()[console.getOutput().size() - 1].kind, Console::Line::Kind::Error);

    broken.setBlocked(true);
    EXPECT_FALSE(console.run("broken").ok);
    broken.disconnect();
    EXPECT_FALSE(console.hasCommand("broken"));
    EXPECT_EQ(console.run("help").output, "gold <integer> [word]  Adds gold.\nhelp [word]  Lists the commands, or shows how to type one.");
    EXPECT_EQ(console.run("help gold").output, "gold <integer> [word]\nAdds gold.");
    EXPECT_TRUE(console.removeCommand("gold"));
    EXPECT_FALSE(gold.isConnected());
}

TEST_F(ConsoleTest, KeepsABoundedHistoryAndCompletesLines) {
    (void)console.run("gold 1");
    (void)console.run("gold 1");
    (void)console.run("help");
    ASSERT_EQ(console.getHistory().size(), 2U);
    EXPECT_EQ(console.getHistory()[0], "gold 1");
    for (std::size_t index = 0; index < Console::kHistorySize + 5; ++index) {
        (void)console.run(std::format("gold {}", index));
    }
    EXPECT_EQ(console.getHistory().size(), Console::kHistorySize);
    EXPECT_EQ(console.getHistory().back(), std::format("gold {}", Console::kHistorySize + 4));

    const core::Connection god = console.addCommand({.name = "god", .help = "Turns damage off.", .parameters = {{.kind = Console::Kind::Boolean}}, .handler = [](const core::Json&) { return std::string(); }});
    EXPECT_EQ(console.complete("g"), (std::vector<std::string>{"god ", "gold "}));
    EXPECT_EQ(console.complete(""), (std::vector<std::string>{"god ", "gold ", "help "}));
    EXPECT_EQ(console.complete("god "), (std::vector<std::string>{"god false", "god true"}));
    EXPECT_EQ(console.complete("god t"), std::vector<std::string>{"god true"});
    EXPECT_TRUE(console.complete("gold 5").empty());
    EXPECT_TRUE(console.complete("god \"t").empty());
}

TEST(ConsoleLuaTest, RunsTheCommandsOfLuaUntilTheirOwnerEnds) {
    test::EngineFixture fixture;
    // clang-format off
    fixture.runLua(R"(
        debugging = require('haylen.debug')
        gold = 0
        owner = {}
        debugging.addCommand('gold', function(amount, reason)
            gold = gold + amount
            return 'Gold ' .. gold .. (reason and ' for ' .. reason or '')
        end, {help = 'Adds gold.', args = {'integer', 'string?'}, owner = owner})
        debugging.addCommand('crash', function() error('The shop is closed.') end)
        debugging.addCommand('table', function() return {} end)
    )");
    // clang-format on
    EXPECT_EQ(fixture.lua("local ok, output = debugging.runCommand('gold 50 quest') return tostring(ok) .. ' ' .. output"), "true Gold 50 for quest");
    EXPECT_EQ(fixture.lua("local ok, output = debugging.runCommand('gold x') return tostring(ok) .. ' ' .. output"), "false The command \"gold\" needs an integer as its first argument, not \"x\". Type it as \"gold <integer> [word]\".");
    EXPECT_NE(fixture.lua("local ok, output = debugging.runCommand('crash') return output").find("The shop is closed."), std::string::npos);
    EXPECT_EQ(fixture.lua("local ok, output = debugging.runCommand('table') return output"), "A console command returns a string, a number or nothing.");
    EXPECT_EQ(fixture.lua("return debugging.completeCommand('go')[1]"), "gold ");
    EXPECT_EQ(fixture.lua("local list = debugging.commandHistory() return #list .. ' ' .. list[1]"), "4 gold 50 quest");
    EXPECT_EQ(fixture.lua("for _, command in ipairs(debugging.commands()) do if command.name == 'gold' then return command.usage .. ' ' .. command.help end end"), "gold <integer> [word] Adds gold.");
    EXPECT_NE(fixture.lua("debugging.addCommand('x', print, {args = {'colour'}})").find("The argument kind \"colour\" is unknown."), std::string::npos);
    EXPECT_NE(fixture.lua("debugging.addCommand('x', print, {sound = true})").find("Unknown option \"sound\"."), std::string::npos);

    fixture.runLua("owner = nil collectgarbage() collectgarbage()");
    fixture.frames(1);
    EXPECT_EQ(fixture.lua("local ok, output = debugging.runCommand('gold 1') return output"), "There is no command \"gold\". Type \"help\" to list the commands.");
}

TEST(ConsoleLuaTest, OpensOnlyInDevelopmentOrWhenTheAppTurnsItOn) {
    {
        test::EngineFixture fixture;
        plugins::DebugPlugin& plugin = fixture.engine().getPlugin<plugins::DebugPlugin>();
        EXPECT_FALSE(plugin.isConsoleEnabled());
        EXPECT_NE(fixture.lua("require('haylen.debug').setConsoleVisible(true)").find("The console is off outside development runs."), std::string::npos);
        EXPECT_FALSE(plugin.getConsole().hasCommand("lua"));

        std::string answer;
        platform::ConsoleRelay::submit("help", [&answer](const Console::Result& result) { answer = result.output; });
        fixture.frames(1);
        EXPECT_EQ(answer, "The console is off outside development runs. Set \"debug.console\" to true in \"app.json\" to turn it on.");
        EXPECT_EQ(fixture.lua("local ok, output = require('haylen.debug').runCommand('timeScale 0.5') return output"), "The time scale is 0.5.");
        EXPECT_DOUBLE_EQ(fixture.engine().getClock().getTimeScale(), 0.5);
    }
    {
        test::EngineFixture fixture({{"app.json", R"({"name": "Console", "identifier": "dev.haylen.console", "debug": {"console": true}})"}});
        plugins::DebugPlugin& plugin = fixture.engine().getPlugin<plugins::DebugPlugin>();
        std::string answer;
        platform::ConsoleRelay::submit("pause true", [&answer](const Console::Result& result) { answer = result.output; });
        fixture.frames(1);
        EXPECT_EQ(answer, "The app is paused.");
        EXPECT_TRUE(fixture.engine().isPaused());

        platform::Event touch;
        touch.type = platform::Event::Type::TouchBegan;
        touch.touchCount = plugins::DebugPlugin::kConsoleTouches;
        fixture.engine().handleEvent(touch);
        EXPECT_TRUE(plugin.isConsoleVisible());
        fixture.frames(2);
        platform::Event key;
        key.type = platform::Event::Type::KeyDown;
        key.key = input::Key::GraveAccent;
        fixture.engine().handleEvent(key);
        EXPECT_FALSE(plugin.isConsoleVisible());
        EXPECT_FALSE(plugin.getConsole().hasCommand("lua"));
    }
    {
        test::EngineFixture fixture({}, nullptr, {.development = true});
        fixture.runLua("player = {speed = 220}");
        EXPECT_EQ(fixture.lua("local ok, output = require('haylen.debug').runCommand('lua player.speed * 2') return output"), "440");
        EXPECT_EQ(fixture.lua("require('haylen.debug').runCommand('lua player.speed = 300') return player.speed"), "300");
        EXPECT_EQ(fixture.lua("local ok, output = require('haylen.debug').runCommand('lua (') return tostring(ok)"), "false");
        EXPECT_EQ(fixture.lua("local ok, output = require('haylen.debug').runCommand('draw physics') return output"), "The drawing \"physics\" is on.");
    }
}

} // namespace haylen::debug
