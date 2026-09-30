#include <gtest/gtest.h>

#include <stdexcept>
#include <string>
#include <vector>

#include "haylen/ai/StateMachine.hpp"
#include "support/EngineFixture.hpp"

namespace haylen::ai {

class StateMachineTest : public ::testing::Test {
  protected:
    // Records every callback of a state under its name.
    [[nodiscard]] static StateMachine::Callbacks makeRecorder(std::vector<std::string>& log, const std::string& name) {
        // clang-format off
        return {
            .enter = [&log, name] { log.push_back("enter " + name); },
            .update = [&log, name](float deltaSeconds) { log.push_back("update " + name + " " + std::to_string(static_cast<int>(deltaSeconds * 10.0F))); },
            .exit = [&log, name] { log.push_back("exit " + name); },
        };
        // clang-format on
    }
};

TEST_F(StateMachineTest, EntersUpdatesAndLeavesStates) {
    std::vector<std::string> log;
    ai::StateMachine machine;
    machine.add("idle", makeRecorder(log, "idle"));
    machine.add("walk", makeRecorder(log, "walk"));
    machine.add("quiet", {});
    machine.onChange = [&log](std::string_view from, std::string_view to) { log.push_back(std::string(from) + ">" + std::string(to)); };

    machine.update(1.0F);
    EXPECT_TRUE(machine.getCurrent().empty());
    EXPECT_TRUE(log.empty());

    machine.change("idle");
    machine.update(0.5F);
    machine.update(0.25F);
    EXPECT_FLOAT_EQ(machine.getElapsed(), 0.75F);
    machine.change("walk");
    EXPECT_EQ(machine.getElapsed(), 0.0F);
    machine.change("walk");
    machine.change("quiet");
    machine.update(0.5F);

    const std::vector<std::string> expected{">idle", "enter idle", "update idle 5", "update idle 2", "exit idle", "idle>walk", "enter walk", "exit walk", "walk>walk", "enter walk", "exit walk", "walk>quiet"};
    EXPECT_EQ(log, expected);
    EXPECT_EQ(machine.getCurrent(), "quiet");
    EXPECT_EQ(machine.getPrevious(), "walk");
    EXPECT_TRUE(machine.has("idle"));
    EXPECT_FALSE(machine.has("run"));
}

TEST_F(StateMachineTest, QueuesChangesRequestedDuringTransitions) {
    std::vector<std::string> log;
    ai::StateMachine machine;
    // clang-format off
    machine.add("a", {
        .enter = [&] {
            log.push_back("enter a");
            machine.change("b");
            machine.change("c");
        },
        .update = {},
        .exit = [&] { log.push_back("exit a"); },
    });
    // clang-format on
    machine.add("b", {.enter = [&] { log.push_back("enter b"); }, .update = {}, .exit = [&] { log.push_back("exit b"); }});
    machine.add("c", {.enter = [&] { log.push_back("enter c"); }, .update = {}, .exit = {}});

    machine.change("a");
    EXPECT_EQ(log, (std::vector<std::string>{"enter a", "exit a", "enter b", "exit b", "enter c"}));
    EXPECT_EQ(machine.getCurrent(), "c");
    EXPECT_EQ(machine.getPrevious(), "b");
}

TEST_F(StateMachineTest, RejectsBadStatesAndRecoversFromFailures) {
    ai::StateMachine machine;
    machine.add("safe", {});
    bool fail = true;
    // clang-format off
    machine.add("fragile", {
        .enter = [&] {
            machine.change("safe");
            if (fail) {
                throw std::runtime_error("broken");
            }
        },
        .update = {},
        .exit = {},
    });
    // clang-format on

    EXPECT_THROW(machine.add("safe", {}), std::invalid_argument);
    EXPECT_THROW(machine.add("", {}), std::invalid_argument);
    EXPECT_THROW(machine.change("missing"), std::invalid_argument);

    EXPECT_THROW(machine.change("fragile"), std::runtime_error);
    EXPECT_EQ(machine.getCurrent(), "fragile");

    // The change queued before the failure was dropped, so the next change starts clean.
    fail = false;
    machine.change("safe");
    EXPECT_EQ(machine.getCurrent(), "safe");
    EXPECT_EQ(machine.getPrevious(), "fragile");
}

TEST(AiLuaTest, RunsLuaStatesWithArguments) {
    test::EngineFixture fixture;
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

} // namespace haylen::ai
