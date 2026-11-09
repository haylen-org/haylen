#include <gtest/gtest.h>

#include <algorithm>
#include <format>
#include <stdexcept>
#include <string>
#include <vector>

#include "haylen/core/Engine.hpp"
#include "haylen/core/FrameClock.hpp"
#include "haylen/debug/Console.hpp"
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

    broken.setBlocked(true);
    EXPECT_FALSE(console.run("broken").ok);
    broken.disconnect();
    EXPECT_FALSE(console.hasCommand("broken"));
    EXPECT_EQ(console.run("help").output, "gold <integer> [word]  Adds gold.\nhelp [word]            Lists the commands, or shows how to type one.");
    EXPECT_EQ(console.run("help gold").output, "gold <integer> [word]\nAdds gold.");
    EXPECT_TRUE(console.removeCommand("gold"));
    EXPECT_FALSE(gold.isConnected());
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
    EXPECT_EQ(fixture.lua("for _, command in ipairs(debugging.commands()) do if command.name == 'gold' then return command.usage .. ' ' .. command.help end end"), "gold <integer> [word] Adds gold.");
    EXPECT_NE(fixture.lua("debugging.addCommand('x', print, {args = {'colour'}})").find("The argument kind \"colour\" is unknown."), std::string::npos);
    EXPECT_NE(fixture.lua("debugging.addCommand('x', print, {sound = true})").find("Unknown option \"sound\"."), std::string::npos);

    fixture.runLua("owner = nil collectgarbage() collectgarbage()");
    fixture.frames(1);
    EXPECT_EQ(fixture.lua("local ok, output = debugging.runCommand('gold 1') return output"), "There is no command \"gold\". Type \"help\" to list the commands.");
}

TEST(ConsoleLuaTest, RunsLinesFromOutsideOnlyInDevelopmentOrWhenTheAppTurnsItOn) {
    {
        test::EngineFixture fixture;
        plugins::DebugPlugin& plugin = fixture.engine().getPlugin<plugins::DebugPlugin>();
        EXPECT_FALSE(plugin.isConsoleEnabled());
        EXPECT_FALSE(plugin.getConsole().hasCommand("lua"));

        std::string answer;
        core::Json commands;
        platform::ConsoleRelay::submit("help", [&answer](const Console::Result& result) { answer = result.output; });
        platform::ConsoleRelay::requestCommands([&commands](const core::Json& list) { commands = list; });
        fixture.frames(1);
        EXPECT_EQ(answer, "The console is off outside development runs. Set \"debug.console\" to true in \"app.json\" to turn it on.");
        EXPECT_EQ(commands, core::Json::array());
        EXPECT_EQ(fixture.lua("local ok, output = require('haylen.debug').runCommand('timeScale 0.5') return output"), "The time scale is 0.5.");
        EXPECT_DOUBLE_EQ(fixture.engine().getClock().getTimeScale(), 0.5);
    }
    {
        test::EngineFixture fixture({{"app.json", R"({"name": "Console", "identifier": "com.haylen.console", "debug": {"console": true}})"}});
        std::string answer;
        core::Json commands;
        platform::ConsoleRelay::submit("pause true", [&answer](const Console::Result& result) { answer = result.output; });
        platform::ConsoleRelay::requestCommands([&commands](const core::Json& list) { commands = list; });
        fixture.frames(1);
        EXPECT_EQ(answer, "The app is paused.");
        EXPECT_TRUE(fixture.engine().isPaused());

        const auto draw = std::ranges::find(commands, "draw", [](const core::Json& command) { return command.at("name").get<std::string>(); });
        ASSERT_NE(draw, commands.end());
        EXPECT_EQ(draw->at("usage"), "draw [word] [boolean]");
        EXPECT_EQ(draw->at("arguments"), (core::Json{"string?", "boolean?"}));
    }
}

TEST(ConsoleLuaTest, CyclesTheStatisticsAndSwitchesEveryDrawingWithoutArguments) {
    test::EngineFixture fixture({}, nullptr, {.development = true});
    plugins::DebugPlugin& plugin = fixture.engine().getPlugin<plugins::DebugPlugin>();
    fixture.runLua("player = {speed = 220} debugging = require('haylen.debug')");
    EXPECT_EQ(fixture.lua("local ok, output = debugging.runCommand('lua player.speed * 2') return output"), "440");
    EXPECT_EQ(fixture.lua("debugging.runCommand('lua player.speed = 300') return player.speed"), "300");
    EXPECT_EQ(fixture.lua("local ok, output = debugging.runCommand('lua (') return tostring(ok)"), "false");

    EXPECT_EQ(fixture.lua("local ok, output = debugging.runCommand('stats') return output"), "The statistics are \"compact\".");
    EXPECT_EQ(fixture.lua("local ok, output = debugging.runCommand('stats') return output"), "The statistics are \"full\".");
    EXPECT_EQ(fixture.lua("local ok, output = debugging.runCommand('stats') return output"), "The statistics are \"off\".");
    EXPECT_EQ(fixture.lua("local ok, output = debugging.runCommand('draw physics') return output"), "The drawing \"physics\" is on.");
    EXPECT_EQ(fixture.lua("local ok, output = debugging.runCommand('draw') return output"), "The drawings are off.");
    EXPECT_EQ(fixture.lua("local ok, output = debugging.runCommand('draw') return output"), "The drawings are on.");
    EXPECT_TRUE(plugin.isDrawing("bounds"));
}

} // namespace haylen::debug
