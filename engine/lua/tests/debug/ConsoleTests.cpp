#include <gtest/gtest.h>

#include <algorithm>
#include <string>

#include "haylen/core/FrameClock.hpp"
#include "haylen/debug/Console.hpp"
#include "haylen/plugins/DebugPlugin.hpp"
#include "platform/ConsoleRelay.hpp"
#include "support/LuaFixture.hpp"

namespace haylen::lua::debug {

TEST(ConsoleLuaTest, RunsTheCommandsOfLuaUntilTheirOwnerEnds) {
    test::LuaFixture fixture;
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
        test::LuaFixture fixture;
        haylen::plugins::DebugPlugin& plugin = fixture.engine().getPlugin<haylen::plugins::DebugPlugin>();
        EXPECT_FALSE(plugin.isConsoleEnabled());
        EXPECT_FALSE(plugin.getConsole().hasCommand("lua"));

        std::string answer;
        haylen::core::Json commands;
        haylen::platform::ConsoleRelay::submit("help", [&answer](const haylen::debug::Console::Result& result) { answer = result.output; });
        haylen::platform::ConsoleRelay::requestCommands([&commands](const haylen::core::Json& list) { commands = list; });
        fixture.frames(1);
        EXPECT_EQ(answer, "The console is off outside development runs. Set \"debug.console\" to true in \"haylen.json\" to turn it on.");
        EXPECT_EQ(commands, haylen::core::Json::array());
        EXPECT_EQ(fixture.lua("local ok, output = require('haylen.debug').runCommand('timeScale 0.5') return output"), "The time scale is 0.5.");
        EXPECT_DOUBLE_EQ(fixture.engine().getClock().getTimeScale(), 0.5);
    }

    {
        test::LuaFixture fixture({{"haylen.json", R"({"name": "Console", "identifier": "com.haylen.console", "debug": {"console": true}})"}});
        std::string answer;
        haylen::core::Json commands;
        haylen::platform::ConsoleRelay::submit("pause true", [&answer](const haylen::debug::Console::Result& result) { answer = result.output; });
        haylen::platform::ConsoleRelay::requestCommands([&commands](const haylen::core::Json& list) { commands = list; });
        fixture.frames(1);
        EXPECT_EQ(answer, "The app is paused.");
        EXPECT_TRUE(fixture.engine().isPaused());

        const auto draw = std::ranges::find(commands, "draw", [](const haylen::core::Json& command) { return command.at("name").get<std::string>(); });
        ASSERT_NE(draw, commands.end());
        EXPECT_EQ(draw->at("usage"), "draw [word] [boolean]");
        EXPECT_EQ(draw->at("arguments"), (haylen::core::Json{"string?", "boolean?"}));
    }
}

TEST(ConsoleLuaTest, CyclesTheStatisticsAndSwitchesEveryDrawingWithoutArguments) {
    test::LuaFixture fixture({}, nullptr, {.development = true});
    haylen::plugins::DebugPlugin& plugin = fixture.engine().getPlugin<haylen::plugins::DebugPlugin>();
    fixture.runLua("player = {speed = 220} debugging = require('haylen.debug')");
    EXPECT_EQ(fixture.lua("local ok, output = debugging.runCommand('lua player.speed * 2') return output"), "440");
    EXPECT_EQ(fixture.lua("debugging.runCommand('lua player.speed = 300') return player.speed"), "300");
    EXPECT_EQ(fixture.lua("local ok, output = debugging.runCommand('lua (') return tostring(ok)"), "false");

    EXPECT_EQ(fixture.lua("local ok, output = debugging.runCommand('stats') return output"), "The statistics are \"compact\".");
    EXPECT_EQ(fixture.lua("local ok, output = debugging.runCommand('stats') return output"), "The statistics are \"full\".");
    EXPECT_EQ(fixture.lua("local ok, output = debugging.runCommand('stats') return output"), "The statistics are \"off\".");
    EXPECT_EQ(fixture.lua("local ok, output = debugging.runCommand('draw physics') return output"), "The drawing \"physics\" is on.");
    EXPECT_EQ(fixture.lua("local ok, output = debugging.runCommand('draw floor') return tostring(ok) .. ' ' .. output"), "false There is no drawing \"floor\". The drawings are \"bounds\" and \"physics\".");
    EXPECT_FALSE(plugin.isDrawing("floor"));
    EXPECT_EQ(fixture.lua("local ok, output = debugging.runCommand('draw') return output"), "The drawings are off.");
    EXPECT_EQ(fixture.lua("local ok, output = debugging.runCommand('draw') return output"), "The drawings are on.");
    EXPECT_TRUE(plugin.isDrawing("bounds"));
}

TEST(ConsoleLuaTest, NamesTheArgumentsThatAreNotAList) {
    test::LuaFixture fixture;
    EXPECT_NE(fixture.lua("require('haylen.debug').addCommand('gold', function() end, {args = 1})").find("The option \"args\" of \"addCommand\" is invalid: table expected, got number."), std::string::npos);
}

} // namespace haylen::lua::debug
