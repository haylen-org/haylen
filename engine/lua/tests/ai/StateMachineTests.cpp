#include <gtest/gtest.h>

#include <string>

#include "support/LuaFixture.hpp"

namespace haylen::lua::ai {

TEST(AiLuaTest, RunsLuaStatesWithArguments) {
    test::LuaFixture fixture;
    // clang-format off
    fixture.runLua(R"(
        ai = require('haylen.ai')
        log = {}
        local function record(text) log[#log + 1] = text end
        machine = ai.newStateMachine({
            idle = {
                enter = function(m, reason) record('enter idle ' .. tostring(reason)) end,
                update = function(m, dt) record('update idle ' .. dt) if dt > 1 then m:change('chase', 'player', 3) end end,
                exit = function(m) record('exit idle') end,
            },
            chase = {
                enter = function(m, target, speed) record('enter chase ' .. target .. ' ' .. speed) if speed > 5 then m:change('idle', 'tired') end end,
                exit = function(m) record('exit chase') end,
            },
            rest = {},
        })
        machine.onChange = function(m, from, to) record(tostring(from) .. '>' .. to) end
    )");
    // clang-format on

    EXPECT_EQ(fixture.lua("return tostring(machine.current) .. ' ' .. tostring(machine.previous)"), "nil nil");
    EXPECT_EQ(fixture.lua("machine:change('idle', 'start') machine:update(0.5) machine:update(2) return table.concat(log, ', ')"), "nil>idle, enter idle start, update idle 0.5, update idle 2.0, exit idle, idle>chase, enter chase player 3");
    EXPECT_EQ(fixture.lua("return machine.current .. ' ' .. machine.previous .. ' ' .. machine.elapsed"), "chase idle 0.0");

    fixture.runLua("log = {} machine:change('chase', 'goblin', 9)");
    EXPECT_EQ(fixture.lua("return table.concat(log, ', ')"), "exit chase, chase>chase, enter chase goblin 9, exit chase, chase>idle, enter idle tired");
    EXPECT_EQ(fixture.lua("machine:change('rest') machine:update(1.5) return machine.current .. ' ' .. machine.elapsed .. ' ' .. tostring(machine:has('rest')) .. tostring(machine:has('fly'))"), "rest 1.5 truefalse");

    EXPECT_NE(fixture.lua("machine:change('fly')").find("no state named \"fly\""), std::string::npos);
    EXPECT_NE(fixture.lua("ai.newStateMachine({idle = 3})").find("each state must be a table"), std::string::npos);
    EXPECT_NE(fixture.lua("ai.newStateMachine({[1] = {}})").find("state names must be strings"), std::string::npos);
    EXPECT_NE(fixture.lua("machine.onChange = 5").find("error: "), std::string::npos);

    // A failing enter leaves its queued change and arguments behind, and the next change still gets its own arguments.
    // clang-format off
    fixture.runLua(R"(
        seen = {}
        fragile = ai.newStateMachine({
            broken = {enter = function(m) m:change('calm', 'stale') error('enter failed') end},
            calm = {enter = function(m, word) seen[#seen + 1] = word end},
        })
    )");
    // clang-format on
    EXPECT_NE(fixture.lua("fragile:change('broken')").find("enter failed"), std::string::npos);
    EXPECT_EQ(fixture.lua("fragile:change('calm', 'fresh') return table.concat(seen, ',') .. ' ' .. fragile.current"), "fresh calm");
    EXPECT_EQ(fixture.lua("machine.onChange = nil machine:change('idle') return tostring(machine.onChange)"), "nil");
}

TEST(AiLuaTest, ChangesQueuedByAnEnterKeepTheirOwnArguments) {
    test::LuaFixture fixture;
    // clang-format off
    fixture.runLua(R"(
        ai = require('haylen.ai')
        log = {}
        machine = ai.newStateMachine({
            start = {enter = function(m) m:change('walk', 'north') m:change('run', 'south', 2) end},
            walk = {enter = function(m, heading) log[#log + 1] = 'walk ' .. heading end},
            run = {enter = function(m, heading, speed) log[#log + 1] = 'run ' .. heading .. ' ' .. speed end},
        })
    )");
    // clang-format on
    EXPECT_EQ(fixture.lua("machine:change('start') return table.concat(log, ', ') .. ' ' .. machine.current"), "walk north, run south 2 run");
}

// The machine reads the callbacks from the table of states when it calls them, so edits of that table reach it, and a state whose table is gone changes without callbacks.
TEST(AiLuaTest, ReadsTheCallbacksOfStatesFromTheirTable) {
    test::LuaFixture fixture;
    // clang-format off
    fixture.runLua(R"(
        log = {}
        states = {idle = {enter = function() log[#log + 1] = 'first' end}, gone = {enter = function() log[#log + 1] = 'gone' end}}
        machine = require('haylen.ai').newStateMachine(states)
    )");
    // clang-format on
    EXPECT_EQ(fixture.lua("states.idle.enter = function() log[#log + 1] = 'second' end machine:change('idle') return table.concat(log, ', ')"), "second");
    EXPECT_EQ(fixture.lua("states.gone = nil machine:change('gone') return table.concat(log, ', ') .. ' ' .. machine.current"), "second gone");
}

} // namespace haylen::lua::ai
