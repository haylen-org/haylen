#include <gtest/gtest.h>

#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include "haylen/2d/animation/AnimatedBatch.hpp"
#include "haylen/2d/animation/Animation.hpp"
#include "haylen/2d/animation/Animator.hpp"
#include "haylen/2d/animation/StateGraph.hpp"
#include "haylen/2d/animation/StateGraphPlayer.hpp"
#include "haylen/core/Engine.hpp"
#include "haylen/graphics/Device.hpp"
#include "haylen/graphics/Image.hpp"
#include "support/EngineFixture.hpp"
#include "support/TestFiles.hpp"

namespace haylen::animation2d {

class StateGraphTest : public ::testing::Test {
  protected:
    static constexpr const char* kGraph = R"json({
        "parameters": {"speed": 0, "grounded": true, "attack": "trigger", "facing": [0, 1]},
        "initial": "idle",
        "states": {
            "idle": {"directions": {"down": "idleDown", "up": "idleUp", "left": "idleLeft", "right": "idleRight"}, "by": "facing"},
            "run": {"directions": {"down": "runDown", "up": "runUp", "left": "runLeft", "right": "runRight"}, "by": "facing"},
            "slash": {"animation": "slash"},
            "fall": {"animation": "fall"}
        },
        "transitions": [
            {"from": "any", "to": "fall", "when": "not grounded"},
            {"from": "fall", "to": "idle", "when": "grounded"},
            {"from": "any", "to": "slash", "when": "attack and grounded"},
            {"from": "slash", "to": "idle", "atEnd": true},
            {"from": "idle", "to": "run", "when": "speed > 0.1"},
            {"from": "run", "to": "idle", "when": "speed <= 0.1 or (facing.x == 0 and facing.y == 0)"}
        ]
    })json";

    // An animator with every animation of the graph, the slash once and the others looping.
    [[nodiscard]] static std::shared_ptr<Animator> makeAnimator(const graphics::Texture& texture) {
        auto animator = std::make_shared<Animator>();
        for (const char* name : {"idleDown", "idleUp", "idleLeft", "idleRight", "runDown", "runUp", "runLeft", "runRight", "fall"}) {
            animator->add(name, Animation::fromGrid(texture, {.frameSize = {16.0F, 16.0F}, .framesPerSecond = 10.0F}));
        }
        animator->add("slash", Animation::fromGrid(texture, {.frameSize = {16.0F, 16.0F}, .framesPerSecond = 10.0F, .loop = Animation::Loop::Once}));
        return animator;
    }

    [[nodiscard]] static std::string error(const char* description) {
        try {
            (void)StateGraph::parse(core::Json::parse(description));
        } catch (const std::invalid_argument& failure) {
            return failure.what();
        }
        return {};
    }
};

TEST_F(StateGraphTest, MovesBetweenStatesByConditionsTriggersAndExitTimes) {
    test::EngineFixture fixture;
    const graphics::Texture texture = fixture.engine().getGraphics().createTexture(graphics::Image(64, 16));
    const auto graph = std::make_shared<StateGraph>(StateGraph::parse(core::Json::parse(kGraph)));
    const std::shared_ptr<Animator> animator = makeAnimator(texture);
    StateGraphPlayer player(graph, animator);
    std::vector<std::string> changes;
    player.stateChanged.connect([&changes](std::string_view from, std::string_view to) { changes.push_back(std::string(from) + ">" + std::string(to)); });
    EXPECT_EQ(player.getState(), "idle");
    EXPECT_EQ(animator->getCurrent(), "idleDown");

    player.setNumber("speed", 1.0F);
    player.update(0.016F);
    EXPECT_EQ(player.getState(), "run");
    EXPECT_EQ(animator->getCurrent(), "runDown");

    // A trigger fires one transition and is consumed by it.
    player.trigger("attack");
    player.update(0.016F);
    EXPECT_EQ(player.getState(), "slash");
    player.update(0.016F);
    EXPECT_EQ(player.getState(), "slash");

    // The slash leaves only at its end, and the idle state then sees the speed again.
    animator->update(0.2F);
    player.update(0.016F);
    EXPECT_EQ(player.getState(), "slash");
    animator->update(0.3F);
    player.update(0.016F);
    EXPECT_EQ(player.getState(), "idle");
    player.update(0.016F);
    EXPECT_EQ(player.getState(), "run");

    player.setBoolean("grounded", false);
    player.update(0.016F);
    player.update(0.016F);
    EXPECT_EQ(player.getState(), "fall");
    EXPECT_EQ(changes, (std::vector<std::string>{"idle>run", "run>slash", "slash>idle", "idle>run", "run>fall"}));
    EXPECT_FLOAT_EQ(player.getStateTime(), 0.016F);
}

TEST_F(StateGraphTest, PicksDirectionsWithHysteresisAndKeepsTheTime) {
    test::EngineFixture fixture;
    const graphics::Texture texture = fixture.engine().getGraphics().createTexture(graphics::Image(64, 16));
    const auto graph = std::make_shared<StateGraph>(StateGraph::parse(core::Json::parse(kGraph)));
    const std::shared_ptr<Animator> animator = makeAnimator(texture);
    StateGraphPlayer player(graph, animator);

    player.setVector("facing", {1.0F, 0.0F});
    player.update(0.0F);
    EXPECT_EQ(animator->getCurrent(), "idleRight");

    // Just past the diagonal the direction holds, and well past it the direction turns while the time stays.
    animator->update(0.25F);
    player.setVector("facing", {1.0F, 1.1F});
    player.update(0.0F);
    EXPECT_EQ(animator->getCurrent(), "idleRight");
    player.setVector("facing", {1.0F, 2.0F});
    player.update(0.0F);
    EXPECT_EQ(animator->getCurrent(), "idleDown");
    EXPECT_FLOAT_EQ(animator->getTime(), 0.25F);
    EXPECT_EQ(animator->getFrame(), 2U);

    // A zero vector keeps the direction, and the run state starts facing it.
    player.setVector("facing", {0.0F, 0.0F});
    player.setNumber("speed", 1.0F);
    player.update(0.0F);
    EXPECT_EQ(animator->getCurrent(), "runDown");

    const StateGraph eight = StateGraph::parse(core::Json::parse(R"({"parameters": {"aim": [0, 0]}, "states": {"walk": {"by": "aim", "directions": {"right": "e", "downRight": "se", "down": "s", "downLeft": "sw", "left": "w", "upLeft": "nw", "up": "n", "upRight": "ne"}}}})"));
    std::vector<float> values{-1.0F, -1.0F};
    EXPECT_EQ(eight.pickDirection(eight.getStates()[0], values, 0), static_cast<std::size_t>(StateGraph::Direction::UpLeft));
}

TEST_F(StateGraphTest, DrivesASpriteOfAnAnimatedBatch) {
    test::EngineFixture fixture;
    const graphics::Texture texture = fixture.engine().getGraphics().createTexture(graphics::Image(64, 16));
    const auto batch = std::make_shared<AnimatedBatch>(texture);
    for (const char* name : {"idle", "walk"}) {
        batch->addAnimation(name, Animation::fromGrid(texture, {.frameSize = {16.0F, 16.0F}}));
    }
    (void)batch->add({}, "idle");
    (void)batch->add({}, "idle");
    const auto graph = std::make_shared<StateGraph>(StateGraph::parse(core::Json::parse(R"({"parameters": {"moving": false}, "states": {"idle": {"animation": "idle"}, "walk": {"animation": "walk"}}, "transitions": [{"from": "idle", "to": "walk", "when": "moving"}]})")));
    StateGraphPlayer player(graph, StateGraphPlayer::BatchSprite{.batch = batch, .index = 1});
    player.setBoolean("moving", true);
    player.update(0.0F);
    EXPECT_EQ(batch->getAnimation(1), "walk");
    EXPECT_EQ(batch->getAnimation(0), "idle");
    EXPECT_THROW(StateGraphPlayer(std::make_shared<StateGraph>(StateGraph::parse(core::Json::parse(kGraph))), StateGraphPlayer::BatchSprite{.batch = batch, .index = 0}), std::invalid_argument);
}

TEST_F(StateGraphTest, ExplainsBadDescriptions) {
    EXPECT_EQ(error(R"({"states": {}})"), "A state graph needs at least one state.");
    EXPECT_EQ(error(R"({"states": {"a": {"animation": "a"}}, "transitions": [{"from": "b", "to": "a"}]})"), "The transition 1 starts from the unknown state \"b\".");
    EXPECT_EQ(error(R"({"states": {"a": {"animation": "a"}}, "transitions": [{"to": "a", "when": "speed > 1"}]})"), "The condition \"speed > 1\" of the transition 1 names the unknown parameter \"speed\".");
    EXPECT_EQ(error(R"({"parameters": {"v": [0, 0]}, "states": {"a": {"animation": "a"}}, "transitions": [{"to": "a", "when": "v > 1"}]})"), "The condition \"v > 1\" of the transition 1 reads the vector \"v\" without \".x\" or \".y\".");
    EXPECT_EQ(error(R"({"parameters": {"n": 0}, "states": {"a": {"animation": "a"}}, "transitions": [{"to": "a", "when": "(n > 1"}]})"), "The condition \"(n > 1\" of the transition 1 misses a closing parenthesis.");
    EXPECT_EQ(error(R"({"parameters": {"v": [0, 0]}, "states": {"a": {"directions": {"down": "a"}, "by": "v"}}})"), "The directions of the state \"a\" name the four straight directions, or all eight.");
    EXPECT_EQ(error(R"({"parameters": {"n": "fast"}, "states": {"a": {"animation": "a"}}})"), "The parameter \"n\" must start as a number, a boolean, \"trigger\" or a vector of two numbers.");
    EXPECT_EQ(error(R"({"states": {"a": {"animation": "a"}}, "speed": 1})"), "Unknown option \"speed\".");
}

TEST_F(StateGraphTest, PlaysGraphsFromLua) {
    const std::vector<std::uint8_t> image = test::TestFiles::pngImage(64, 16, 0xFFFFFFFFU);
    test::EngineFixture fixture({{"content/hero.png", std::string(image.begin(), image.end())}, {"content/hero.graph.json", R"({"parameters": {"speed": 0, "jump": "trigger"}, "states": {"idle": {"animation": "idle"}, "run": {"animation": "run"}, "jump": {"animation": "idle"}}, "transitions": [{"from": "any", "to": "jump", "when": "jump"}, {"from": "idle", "to": "run", "when": "speed > 0.5"}]})"}});
    // clang-format off
    EXPECT_EQ(fixture.lua(R"(
        local animation2d = require('haylen.animation2d')
        local assets = require('haylen.assets')
        local sheet = assets.texture('hero.png')
        local animator = animation2d.newAnimator()
        animator:add('idle', animation2d.fromGrid(sheet, {frameWidth = 16, frameHeight = 16}))
        animator:add('run', animation2d.fromGrid(sheet, {frameWidth = 16, frameHeight = 16}))
        local graph = animation2d.newStateGraph('hero.graph.json')
        player = graph:newPlayer(animator)
        local heard = {}
        player:on('stateChanged', function(from, to) heard[#heard + 1] = from .. '>' .. to end, {owner = player})
        player:set('speed', 1)
        player:update(1 / 60)
        player:set('jump', true)
        player:update(1 / 60)
        return table.concat(heard, ',') .. ' ' .. player.state .. ' ' .. player.animation .. ' ' .. player:get('speed') .. ' ' .. #graph.states .. ' ' .. graph.parameters.jump
    )"), "idle>run,run>jump jump idle 1.0 3 trigger");
    // clang-format on
    EXPECT_EQ(fixture.lua("local ok, message = pcall(player.set, player, 'height', 2) return message:match('The state graph.*')"), "The state graph has no parameter \"height\".");
    EXPECT_EQ(fixture.lua("local ok, message = pcall(player.enter, player, 'swim') return message:match('The state graph.*')"), "The state graph has no state \"swim\".");
    EXPECT_EQ(fixture.lua("local animation2d = require('haylen.animation2d') local ok, message = pcall(animation2d.newStateGraph, {states = {a = {animation = 'a'}}, transitions = {{to = 'b'}}}) return message:match('The transition.*')"), "The transition 1 goes to the unknown state \"b\".");
}

} // namespace haylen::animation2d
