#include <gtest/gtest.h>

#include <string>

#include "support/EngineFixture.hpp"

namespace haylen {

class DecisionLuaTest : public ::testing::Test {
  protected:
    void SetUp() override {
        fixture.runLua("ai = require('haylen.ai') m = require('haylen.math')");
    }

    std::string lua(const std::string& source) {
        return fixture.lua(source);
    }

    test::EngineFixture fixture;
};

TEST_F(DecisionLuaTest, TicksBehaviorTreesWithLuaLeaves) {
    // clang-format off
    fixture.runLua(R"(
        log = {}
        guard = ai.newBehaviorTree(ai.selector({
            ai.sequence({
                ai.condition(function(board) return board.enemy ~= nil end),
                ai.action(function(board, dt)
                    board.distance = board.distance - 10 * dt
                    log[#log + 1] = 'chase'
                    if board.distance <= 0 then return 'success' end
                    return 'running'
                end),
            }),
            ai.action(function(board) log[#log + 1] = 'patrol' return true end),
        }), {distance = 15})
    )");
    // clang-format on
    EXPECT_EQ(lua("return guard:tick(1) .. ' ' .. table.concat(log, ',')"), "success patrol");
    EXPECT_EQ(lua("guard.blackboard.enemy = 'player' return guard:tick(1) .. ' ' .. guard:tick(1) .. ' ' .. guard.status"), "running success success");
    EXPECT_EQ(lua("return table.concat(log, ',') .. ' ' .. guard.nodeCount .. ' ' .. guard.time"), "patrol,chase,chase 5 3.0");

    // clang-format off
    fixture.runLua(R"(
        count = 0
        timers = ai.newBehaviorTree(ai.sequence({
            ai.wait(0.5),
            ai.cooldown(ai.action(function() count = count + 1 end), 1),
            ai.repeater(ai.succeeder(ai.inverter(ai.action(function() return false end))), 2),
            ai.retry(ai.condition(function() return count > 0 end), 3),
            ai.timeout(ai.failer(ai.action(function() return 'success' end)), 1),
        }))
    )");
    // clang-format on
    EXPECT_EQ(lua("return timers:tick(0.25) .. ' ' .. timers:tick(0.25) .. ' ' .. count"), "running running 1");
    EXPECT_EQ(lua("return timers:tick(0.25) .. ' ' .. timers:tick(0.25)"), "failure running");
    EXPECT_EQ(lua("timers:reset() return timers.status"), "success");
    EXPECT_EQ(lua("local both = ai.newBehaviorTree(ai.parallel({ai.action(function() return true end), ai.action(function() return 'running' end)}, 1)) return both:tick(0)"), "success");

    fixture.runLua("broken = ai.newBehaviorTree(ai.action(function() return 'maybe' end))");
    EXPECT_NE(lua("broken:tick(0)").find("unknown status 'maybe'"), std::string::npos);
    EXPECT_NE(lua("broken:tick(0)").find("unknown status 'maybe'"), std::string::npos);
    EXPECT_NE(lua("ai.newBehaviorTree({kind = 'loop'})").find("Unknown behavior tree node 'loop'"), std::string::npos);
    EXPECT_NE(lua("ai.newBehaviorTree(ai.parallel({}, 1))").find("parallel node"), std::string::npos);
    EXPECT_NE(lua("local tree tree = ai.newBehaviorTree(ai.action(function() tree:tick(0) end)) tree:tick(0)").find("already ticking"), std::string::npos);
}

TEST_F(DecisionLuaTest, ChoosesByUtility) {
    // clang-format off
    fixture.runLua(R"(
        brain = ai.newUtilitySelector({
            {name = 'attack', considerations = {
                {name = 'health', input = function(bot) return bot.health end, maximum = 100},
                {name = 'near', input = function(bot) return bot.distance end, maximum = 200, curve = {slope = -1, offset = 1}},
            }},
            {name = 'flee', weight = 0.9, considerations = {
                {name = 'danger', input = function(bot) return bot.health end, maximum = 100, curve = {shape = 'polynomial', slope = -1, exponent = 2, offset = 1}},
            }},
        })
        bot = {health = 100, distance = 50}
    )");
    // clang-format on
    EXPECT_EQ(lua("return brain:choose(bot)"), "attack");
    EXPECT_EQ(lua("bot.health = 20 local name, score = brain:choose(bot) return name .. ' ' .. string.format('%.3f', score)"), "flee 0.864");
    EXPECT_EQ(lua("return string.format('%.3f', brain:score('flee', bot)) .. ' ' .. string.format('%.3f', brain:score(1, bot))"), "0.864 0.214");
    EXPECT_EQ(lua("return brain:choose(bot, m.random(1), 1)"), "flee");
    EXPECT_EQ(lua("return table.concat(brain.options, ',')"), "attack,flee");
    EXPECT_EQ(lua("bot.health = 100 bot.distance = 200 return tostring(brain:choose(bot))"), "nil");
    EXPECT_NE(lua("brain:score('sleep', bot)").find("unknown option"), std::string::npos);
    EXPECT_NE(lua("ai.newUtilitySelector({{name = 'x', considerations = {{input = function() return 0 end, curve = {shape = 'wave'}}}}})").find("unknown value 'wave'"), std::string::npos);
}

TEST_F(DecisionLuaTest, SpreadsInfluence) {
    // clang-format off
    fixture.runLua(R"(
        threat = ai.newInfluenceMap({columns = 20, rows = 10, cellSize = 10, x = 100, y = 0})
        threat:stamp(155, 55, 1, 30)
        threat:propagate(0.5, 0.5)
        allies = ai.newInfluenceMap({columns = 20, rows = 10, cellSize = 10, x = 100, y = 0})
        allies:fill(1)
        allies:add(threat, -1)
        allies:scale(2)
    )");
    // clang-format on
    EXPECT_EQ(lua("return threat.columns .. ' ' .. threat.rows .. ' ' .. threat.cellSize .. ' ' .. threat.origin.x .. ' ' .. #threat:values()"), "20 10 10.0 100.0 200");
    EXPECT_EQ(lua("local x, y, value = threat:highest(150, 50, 100) return x .. ' ' .. y .. ' ' .. tostring(value > 0.5)"), "155.0 55.0 true");
    EXPECT_EQ(lua("local x, y = threat:lowest(285, 5, 1) return x .. ' ' .. y .. ' ' .. tostring(threat:lowest(-500, 0, 1))"), "285.0 5.0 nil");
    EXPECT_EQ(lua("return tostring(threat:sample(155, 55) > 0.5) .. ' ' .. tostring(threat:get(5, 5) > threat:get(9, 5)) .. ' ' .. allies:get(0, 0)"), "true true 2.0");
    EXPECT_EQ(lua("threat:set(0, 0, 3) local x, y = threat:cellCenter(0, 0) return threat:get(0, 0) .. ' ' .. x .. ' ' .. y"), "3.0 105.0 5.0");
    EXPECT_EQ(lua("threat:stamp(105, 5, 1, 20, 'constant') return threat:get(0, 0)"), "4.0");
    EXPECT_NE(lua("threat:stamp(0, 0, 1, 10, 'spiky')").find("unknown value 'spiky'"), std::string::npos);
    EXPECT_NE(lua("threat:get(20, 0)").find("outside the influence map"), std::string::npos);
    EXPECT_NE(lua("ai.newInfluenceMap({columns = 0, rows = 1})").find("at least one cell"), std::string::npos);
}

} // namespace haylen
