#include <gtest/gtest.h>

#include <string>

#include "support/LuaFixture.hpp"

namespace haylen::lua::input {

TEST(CommandBufferLuaTest, TypesCommandsFromMasksAndMaps) {
    test::LuaFixture fixture;
    // clang-format off
    fixture.runLua(R"(
        input = require('haylen.input')
        buttons = {'defend', 'up', 'down', 'left', 'right', 'jump', 'attack'}
        commands = input.newCommandBuffer({
            buttons = buttons,
            lifetime = 10,
            lifetimes = {up = 1, down = 1, left = 1, right = 1},
            directions = {back = 'left', forward = 'right'},
            commands = {
                {name = 'run', steps = {'forward', 'forward'}, maxTicks = 9, priority = 1},
                {name = 'upSpecial', steps = {'defend', 'up', 'attack'}, priority = 4, lifetime = false},
            },
        })
    )");
    // clang-format on
    EXPECT_EQ(fixture.lua("commands:update(16) commands:update(0) commands:update(16) return commands:peek()"), "run");
    EXPECT_EQ(fixture.lua("commands:update(1) commands:update(2) commands:update(64) for i = 1, 30 do commands:update(0) end return commands:peek()"), "upSpecial");
    EXPECT_EQ(fixture.lua("return tostring(commands:consume('upSpecial')) .. ' ' .. tostring(commands:peek())"), "true nil");
    EXPECT_EQ(fixture.lua("commands.facing = -1 commands.frozen = true return commands.facing .. ' ' .. tostring(commands.frozen) .. ' ' .. commands.tick"), "-1 true 36");
    EXPECT_EQ(fixture.lua("commands:settle(64) commands:update(64) return tostring(commands:peek()) .. ' ' .. tostring(commands:held('attack'))"), "nil true");
    EXPECT_EQ(fixture.lua("commands:clear() return commands:peek()"), "nil");

    fixture.runLua("map = input.newActionMap({actions = {actions = {{name = 'attack', type = 'button', bindings = {'virtual:attack'}}}}})");
    fixture.runLua("input.setVirtualButton('attack', true)");
    fixture.frames(1);
    EXPECT_EQ(fixture.lua("local only = input.newCommandBuffer({buttons = {'attack'}}) only:update(map) return only:peek()"), "attack");

    EXPECT_NE(fixture.lua("input.newCommandBuffer({buttons = {'a'}, commands = {{name = 'b', steps = {'kick'}}}})").find("The command \"b\" names the button \"kick\", which the buffer does not have."), std::string::npos);
    EXPECT_NE(fixture.lua("input.newCommandBuffer({buttons = {'a'}, lifetime = 0})").find("A command lasts a whole number of steps from 1, or false to last until it is consumed."), std::string::npos);
    EXPECT_NE(fixture.lua("input.newCommandBuffer({buttons = {'a'}, lifetimes = {b = 2}})").find("The lifetimes name the button \"b\", which the buffer does not have."), std::string::npos);
    EXPECT_NE(fixture.lua("input.newCommandBuffer({buttons = {'a'}, size = 2})").find("Unknown option \"size\"."), std::string::npos);
    EXPECT_NE(fixture.lua("input.newCommandBuffer({buttons = {'a'}, commands = {{name = 'a', steps = {'a'}}}})").find("The command name \"a\" is used by more than one command."), std::string::npos);
    EXPECT_NE(fixture.lua("return commands:consume('fly')").find("The command buffer has no command \"fly\"."), std::string::npos);
    EXPECT_NE(fixture.lua("return commands:held('fly')").find("The command buffer has no button \"fly\"."), std::string::npos);
    EXPECT_NE(fixture.lua("commands:update(128)").find("The mask of a step presses a button that the command buffer does not have."), std::string::npos);
    EXPECT_NE(fixture.lua("commands:settle(256)").find("The mask of a step presses a button that the command buffer does not have."), std::string::npos);
}

TEST(CommandBufferLuaTest, NamesTheOptionThatIsNotATable) {
    test::LuaFixture fixture;
    EXPECT_NE(fixture.lua("require('haylen.input').newCommandBuffer({buttons = {'a'}, commands = 'none'})").find("The option \"commands\" of \"newCommandBuffer\" is invalid: table expected, got string."), std::string::npos);
}

} // namespace haylen::lua::input
