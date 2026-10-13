#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <initializer_list>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

#include "haylen/input/ActionMap.hpp"
#include "haylen/input/CommandBuffer.hpp"
#include "haylen/input/Input.hpp"
#include "haylen/input/VirtualInput.hpp"
#include "support/EngineFixture.hpp"

namespace haylen::input {

// A buffer with the buttons of a fighter: defend, the four directions, jump and attack, where directions last one step and the other presses ten.
class CommandBufferTest : public ::testing::Test {
  protected:
    enum Button : std::size_t {
        Defend,
        Up,
        Down,
        Left,
        Right,
        Jump,
        Attack,
    };

    [[nodiscard]] static CommandBuffer::Options makeOptions(std::initializer_list<CommandBuffer::Command> commands) {
        return {
            .buttons = {"defend", "up", "down", "left", "right", "jump", "attack"},
            .lifetimes = {10U, 1U, 1U, 1U, 1U, 10U, 10U},
            .commands = commands,
            .directions = std::array<std::size_t, 2>{Left, Right},
        };
    }

    [[nodiscard]] static std::uint32_t mask(std::initializer_list<Button> buttons) {
        std::uint32_t value = 0;
        for (const Button button : buttons) {
            value |= 1U << button;
        }
        return value;
    }

    // Presses the buttons for one step, lets go for the next one and leaves the steps after that empty.
    static void tap(CommandBuffer& buffer, std::initializer_list<Button> buttons, int idleSteps = 0) {
        buffer.update(mask(buttons));
        for (int step = 0; step <= idleSteps; ++step) {
            buffer.update(0);
        }
    }

    [[nodiscard]] static std::string kept(const CommandBuffer& buffer) {
        const std::optional<std::size_t> command = buffer.peek();
        return command ? buffer.getCommandName(*command) : std::string("none");
    }
};

TEST_F(CommandBufferTest, RunsOnADoubleTapWithinItsTicks) {
    CommandBuffer buffer(makeOptions({{.name = "run", .steps = {CommandBuffer::kForward, CommandBuffer::kForward}, .maxTicks = 9U, .priority = 1, .lifetime = 10U}}));
    tap(buffer, {Right}, 7);
    buffer.update(mask({Right}));
    EXPECT_EQ(kept(buffer), "run");

    // A tenth tick between the taps is too late, and another press between them breaks the sequence.
    buffer.clear();
    tap(buffer, {Right}, 8);
    buffer.update(mask({Right}));
    EXPECT_EQ(kept(buffer), "right");
    buffer.clear();
    tap(buffer, {Right});
    tap(buffer, {Up});
    buffer.update(mask({Right}));
    EXPECT_EQ(kept(buffer), "right");
}

TEST_F(CommandBufferTest, KeepsTheHighestPriorityOfAStepUntilALaterPress) {
    CommandBuffer buffer(makeOptions({{.name = "jumpAttack", .steps = {Jump, Attack}, .maxTicks = 0U, .priority = 2, .lifetime = 10U}}));
    buffer.update(mask({Jump, Attack}));
    EXPECT_EQ(kept(buffer), "jumpAttack");
    buffer.update(mask({Jump, Attack, Defend}));
    EXPECT_EQ(kept(buffer), "defend");

    // The chord needs both presses on one step, so a jump and a later attack type the attack alone.
    buffer.clear();
    buffer.update(0);
    tap(buffer, {Jump});
    buffer.update(mask({Attack}));
    EXPECT_EQ(kept(buffer), "attack");
    EXPECT_EQ(buffer.getTick(), 6U);
}

TEST_F(CommandBufferTest, ExpiresByLifetimeExceptWhileFrozen) {
    CommandBuffer buffer(makeOptions({{.name = "upSpecial", .steps = {Defend, Up, Attack}, .priority = 4}}));
    buffer.update(mask({Attack}));
    for (int step = 0; step < 9; ++step) {
        buffer.update(0);
    }
    EXPECT_EQ(kept(buffer), "attack");
    buffer.update(0);
    EXPECT_EQ(kept(buffer), "none");

    // A frozen buffer keeps a long lifetime, while a direction lasts only the step it was typed on.
    buffer.setFrozen(true);
    buffer.update(mask({Attack}));
    for (int step = 0; step < 20; ++step) {
        buffer.update(0);
    }
    EXPECT_EQ(kept(buffer), "attack");
    buffer.update(mask({Left}));
    EXPECT_EQ(kept(buffer), "left");
    buffer.update(0);
    EXPECT_EQ(kept(buffer), "none");

    // A command without a lifetime waits until it is consumed.
    buffer.setFrozen(false);
    tap(buffer, {Defend});
    tap(buffer, {Up});
    buffer.update(mask({Attack}));
    for (int step = 0; step < 50; ++step) {
        buffer.update(0);
    }
    const std::optional<std::size_t> special = buffer.findCommand("upSpecial");
    ASSERT_TRUE(special.has_value());
    EXPECT_EQ(buffer.peek(), special);
    EXPECT_FALSE(buffer.consume(*buffer.findCommand("attack")));
    EXPECT_TRUE(buffer.consume(*special));
    EXPECT_FALSE(buffer.consume(*special));
}

TEST_F(CommandBufferTest, ClearsTheHistoryUnlessTheCommandKeepsIt) {
    CommandBuffer clearing(makeOptions({{.name = "run", .steps = {CommandBuffer::kForward, CommandBuffer::kForward}, .maxTicks = 9U, .lifetime = 10U}}));
    CommandBuffer keeping(makeOptions({{.name = "run", .steps = {CommandBuffer::kForward, CommandBuffer::kForward}, .maxTicks = 9U, .lifetime = 10U, .clearsHistory = false}}));
    for (CommandBuffer* buffer : {&clearing, &keeping}) {
        tap(*buffer, {Right});
        tap(*buffer, {Right});
        buffer->update(mask({Right}));
    }
    EXPECT_EQ(kept(clearing), "right");
    EXPECT_EQ(kept(keeping), "run");
}

TEST_F(CommandBufferTest, FollowsTheFacingAndSettlesHeldButtons) {
    CommandBuffer buffer(makeOptions({{.name = "run", .steps = {CommandBuffer::kForward, CommandBuffer::kForward}, .maxTicks = 9U, .lifetime = 10U}}));
    buffer.setFacing(-1);
    EXPECT_EQ(buffer.getFacing(), -1);
    tap(buffer, {Right});
    buffer.update(mask({Right}));
    EXPECT_EQ(kept(buffer), "right");
    buffer.update(0);
    tap(buffer, {Left});
    buffer.update(mask({Left}));
    EXPECT_EQ(kept(buffer), "run");

    buffer.clear();
    buffer.settle(mask({Attack}));
    buffer.update(mask({Attack}));
    EXPECT_EQ(kept(buffer), "none");
    EXPECT_TRUE(buffer.isHeld(Attack));
    EXPECT_FALSE(buffer.isHeld(Jump));
}

TEST_F(CommandBufferTest, ReadsTheStepsOfAnActionMap) {
    ActionMap map;
    map.define({.name = "attack", .type = ActionMap::Action::Type::Button, .bindings = {*ActionMap::Binding::parse("virtual:attack")}});
    CommandBuffer buffer(makeOptions({}));
    Input input;
    VirtualInput virtualInput;

    // A tap that starts and ends between two steps still types a press.
    virtualInput.setButton("attack", true);
    map.update(input, virtualInput, false);
    virtualInput.setButton("attack", false);
    map.update(input, virtualInput, false);
    map.beginStep(1.0F / 30.0F);
    buffer.update(map);
    EXPECT_EQ(kept(buffer), "attack");
    map.beginStep(1.0F / 30.0F);
    buffer.settle(map);
    EXPECT_FALSE(buffer.isHeld(Attack));
}

TEST_F(CommandBufferTest, RejectsInvalidOptions) {
    // clang-format off
    const auto message = [](CommandBuffer::Options options) {
        try {
            CommandBuffer buffer(std::move(options));
        } catch (const std::invalid_argument& error) {
            return std::string(error.what());
        }
        return std::string();
    };
    // clang-format on
    EXPECT_EQ(message({}), "A command buffer needs from 1 to 32 buttons.");
    EXPECT_EQ(message({.buttons = {"a"}}), "A command buffer needs one lifetime for each button.");
    EXPECT_EQ(message({.buttons = {"a"}, .lifetimes = {0U}}), "A command lasts at least one step.");
    EXPECT_EQ(message({.buttons = {"a"}, .lifetimes = {1U}, .commands = {{.name = "b"}}}), "A command needs a name and at least one step.");
    EXPECT_EQ(message({.buttons = {"a"}, .lifetimes = {1U}, .commands = {{.name = "b", .steps = {3}}}}), "The command \"b\" names a button that the buffer does not have.");
    EXPECT_EQ(message({.buttons = {"a"}, .lifetimes = {1U}, .commands = {{.name = "b", .steps = {CommandBuffer::kForward}}}}), "The command \"b\" names a direction, which needs the directions of the buffer.");
    EXPECT_EQ(message({.buttons = {"a"}, .lifetimes = {1U}, .commands = {{.name = "a", .steps = {0}}}}), "The command name \"a\" is used by more than one command.");
    EXPECT_EQ(message({.buttons = {"a"}, .lifetimes = {1U}, .directions = std::array<std::size_t, 2>{0, 4}}), "The directions of a command buffer must name two of its buttons.");
}

TEST(CommandBufferLuaTest, TypesCommandsFromMasksAndMaps) {
    test::EngineFixture fixture;
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
}

} // namespace haylen::input
