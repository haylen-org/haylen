#include <gtest/gtest.h>

#include <stdexcept>
#include <string>
#include <vector>

#include "haylen/ai/BehaviorTree.hpp"
#include "haylen/ai/Blackboard.hpp"
#include "haylen/ai/InfluenceMap.hpp"
#include "haylen/ai/ResponseCurve.hpp"
#include "haylen/ai/UtilitySelector.hpp"
#include "haylen/math/Random.hpp"

namespace haylen::ai {

using Status = BehaviorTree::Status;

class BehaviorTreeTest : public ::testing::Test {
  protected:
    // An action that runs for the given number of ticks, then returns the result, and records its calls.
    BehaviorTree::Node step(std::string name, int ticks, Status result) {
        auto remaining = std::make_shared<int>(ticks);
        // clang-format off
        return BehaviorTree::action([this, name = std::move(name), remaining, ticks, result](Blackboard&, float) {
            calls.push_back(name);
            if (--*remaining > 0) {
                return Status::Running;
            }
            *remaining = ticks;
            return result;
        });
        // clang-format on
    }

    std::vector<std::string> calls;
};

TEST_F(BehaviorTreeTest, SequencesAndSelectorsResumeTheirRunningChild) {
    BehaviorTree tree(BehaviorTree::sequence({step("aim", 1, Status::Success), step("shoot", 2, Status::Success), step("reload", 1, Status::Failure)}));
    EXPECT_EQ(tree.getNodeCount(), 4U);
    EXPECT_EQ(tree.tick(0.1F), Status::Running);
    EXPECT_EQ(calls, (std::vector<std::string>{"aim", "shoot"}));
    EXPECT_EQ(tree.tick(0.1F), Status::Failure);
    EXPECT_EQ(calls, (std::vector<std::string>{"aim", "shoot", "shoot", "reload"}));
    EXPECT_EQ(tree.getStatus(), Status::Failure);

    calls.clear();
    BehaviorTree fallback(BehaviorTree::selector({step("flee", 1, Status::Failure), step("hide", 2, Status::Success), step("fight", 1, Status::Success)}));
    EXPECT_EQ(fallback.tick(0.1F), Status::Running);
    EXPECT_EQ(fallback.tick(0.1F), Status::Success);
    EXPECT_EQ(calls, (std::vector<std::string>{"flee", "hide", "hide"}));
    EXPECT_EQ(BehaviorTree(BehaviorTree::selector({})).tick(0.1F), Status::Failure);
    EXPECT_EQ(BehaviorTree(BehaviorTree::sequence({})).tick(0.1F), Status::Success);
}

TEST_F(BehaviorTreeTest, DecoratorsReshapeResults) {
    EXPECT_EQ(BehaviorTree(BehaviorTree::inverter(step("a", 1, Status::Success))).tick(0.0F), Status::Failure);
    EXPECT_EQ(BehaviorTree(BehaviorTree::inverter(step("a", 2, Status::Success))).tick(0.0F), Status::Running);
    EXPECT_EQ(BehaviorTree(BehaviorTree::succeeder(step("a", 1, Status::Failure))).tick(0.0F), Status::Success);
    EXPECT_EQ(BehaviorTree(BehaviorTree::failer(step("a", 1, Status::Success))).tick(0.0F), Status::Failure);

    BehaviorTree three(BehaviorTree::repeater(step("patrol", 1, Status::Success), 3));
    EXPECT_EQ(three.tick(0.1F), Status::Running);
    EXPECT_EQ(three.tick(0.1F), Status::Running);
    EXPECT_EQ(three.tick(0.1F), Status::Success);

    int attempts = 0;
    BehaviorTree retry(BehaviorTree::retry(BehaviorTree::condition([&attempts](Blackboard&) { return ++attempts == 3; }), 5));
    EXPECT_EQ(retry.tick(0.1F), Status::Running);
    EXPECT_EQ(retry.tick(0.1F), Status::Running);
    EXPECT_EQ(retry.tick(0.1F), Status::Success);
    BehaviorTree giveUp(BehaviorTree::retry(BehaviorTree::condition([](Blackboard&) { return false; }), 2));
    EXPECT_EQ(giveUp.tick(0.1F), Status::Running);
    EXPECT_EQ(giveUp.tick(0.1F), Status::Failure);
}

TEST_F(BehaviorTreeTest, TimersFollowTheTreeClock) {
    BehaviorTree wait(BehaviorTree::wait(1.0F));
    EXPECT_EQ(wait.tick(0.5F), Status::Running);
    EXPECT_EQ(wait.tick(0.5F), Status::Success);
    EXPECT_FLOAT_EQ(wait.getTime(), 1.0F);

    BehaviorTree cooldown(BehaviorTree::cooldown(step("bark", 1, Status::Success), 1.0F));
    EXPECT_EQ(cooldown.tick(0.25F), Status::Success);
    EXPECT_EQ(cooldown.tick(0.25F), Status::Failure);
    EXPECT_EQ(cooldown.tick(0.5F), Status::Failure);
    EXPECT_EQ(cooldown.tick(0.25F), Status::Success);
    EXPECT_EQ(calls.size(), 2U);

    BehaviorTree timeout(BehaviorTree::timeout(step("chase", 10, Status::Success), 0.3F));
    EXPECT_EQ(timeout.tick(0.2F), Status::Running);
    EXPECT_EQ(timeout.tick(0.2F), Status::Failure);
    EXPECT_EQ(timeout.tick(0.2F), Status::Running);

    cooldown.reset();
    EXPECT_EQ(cooldown.tick(0.1F), Status::Success);
}

TEST_F(BehaviorTreeTest, ParallelNodesCountSuccessesAndFailures) {
    BehaviorTree both(BehaviorTree::parallel({step("walk", 2, Status::Success), step("talk", 3, Status::Success)}, 2));
    EXPECT_EQ(both.tick(0.1F), Status::Running);
    EXPECT_EQ(both.tick(0.1F), Status::Running);
    EXPECT_EQ(both.tick(0.1F), Status::Success);
    // The walk finished on the second tick, so only the talk ran on the third.
    EXPECT_EQ(calls, (std::vector<std::string>{"walk", "talk", "walk", "talk", "talk"}));

    BehaviorTree either(BehaviorTree::parallel({step("a", 1, Status::Failure), step("b", 1, Status::Failure)}, 1));
    EXPECT_EQ(either.tick(0.1F), Status::Failure);
    BehaviorTree first(BehaviorTree::parallel({step("a", 1, Status::Success), step("b", 5, Status::Failure)}, 1));
    EXPECT_EQ(first.tick(0.1F), Status::Success);
}

TEST_F(BehaviorTreeTest, LeavesShareTheBlackboard) {
    // clang-format off
    BehaviorTree tree(BehaviorTree::sequence({
        BehaviorTree::condition([](Blackboard& board) { return board.has("target"); }),
        BehaviorTree::action([](Blackboard& board, float dt) {
            *board.get<float>("distance") -= 10.0F * dt;
            return *board.get<float>("distance") <= 0.0F ? Status::Success : Status::Running;
        }),
    }));
    // clang-format on
    EXPECT_EQ(tree.tick(1.0F), Status::Failure);
    tree.getBlackboard().set("target", std::string("player"));
    tree.getBlackboard().set("distance", 15.0F);
    EXPECT_EQ(tree.tick(1.0F), Status::Running);
    EXPECT_EQ(tree.tick(1.0F), Status::Success);

    Blackboard& board = tree.getBlackboard();
    EXPECT_EQ(*board.get<std::string>("target"), "player");
    EXPECT_EQ(board.get<int>("target"), nullptr);
    EXPECT_EQ(board.size(), 2U);
    board.erase("target");
    EXPECT_FALSE(board.has("target"));
    board.clear();
    EXPECT_EQ(board.size(), 0U);
}

TEST_F(BehaviorTreeTest, RejectsMalformedTrees) {
    BehaviorTree::Node bare = BehaviorTree::inverter(BehaviorTree::wait(1.0F));
    bare.children.clear();
    EXPECT_THROW(BehaviorTree{bare}, std::invalid_argument);
    EXPECT_THROW(BehaviorTree(BehaviorTree::action(nullptr)), std::invalid_argument);
    EXPECT_THROW(BehaviorTree(BehaviorTree::parallel({BehaviorTree::wait(1.0F)}, 2)), std::invalid_argument);
    EXPECT_EQ(BehaviorTree::statusFromName("running"), Status::Running);
    EXPECT_EQ(BehaviorTree::statusName(Status::Failure), "failure");
}

TEST(UtilitySelectorTest, PicksTheOptionWithTheBestScore) {
    float health = 100.0F;
    float distance = 50.0F;
    UtilitySelector selector;
    const std::size_t attack = selector.add({.name = "attack", .considerations = {{.name = "health", .input = [&health] { return health; }, .maximum = 100.0F}, {.name = "near", .input = [&distance] { return distance; }, .maximum = 200.0F, .curve = {.slope = -1.0F, .offset = 1.0F}}}});
    const std::size_t flee = selector.add({.name = "flee", .considerations = {{.name = "danger", .input = [&health] { return health; }, .maximum = 100.0F, .curve = {.shape = ResponseCurve::Shape::Polynomial, .slope = -1.0F, .exponent = 2.0F, .offset = 1.0F}}}, .weight = 0.9F});

    EXPECT_EQ(selector.choose()->index, attack);
    health = 20.0F;
    EXPECT_EQ(selector.choose()->index, flee);
    EXPECT_NEAR(selector.score(flee), 0.9F * 0.96F, 1e-4F);

    // With a tolerance of zero every option with a score can come up.
    math::Random random(2);
    std::vector<int> picks(2);
    for (int pick = 0; pick < 200; ++pick) {
        ++picks[selector.choose(random, 0.0F)->index];
    }
    EXPECT_GT(picks[attack], 0);
    EXPECT_GT(picks[flee], picks[attack]);
    EXPECT_EQ(selector.choose(random, 1.0F)->index, flee);

    health = 100.0F;
    distance = 200.0F;
    EXPECT_FALSE(selector.choose().has_value());
    EXPECT_THROW(selector.add({.considerations = {{.input = [] { return 0.0F; }, .maximum = 0.0F}}}), std::invalid_argument);
}

TEST(ResponseCurveTest, ShapesStayWithinZeroAndOne) {
    for (const auto shape : {ResponseCurve::Shape::Linear, ResponseCurve::Shape::Polynomial, ResponseCurve::Shape::Logistic, ResponseCurve::Shape::Logit, ResponseCurve::Shape::Normal}) {
        const ResponseCurve curve{.shape = shape, .slope = 3.0F, .exponent = 2.0F, .shift = 0.5F};
        for (int step = -2; step <= 12; ++step) {
            const float value = curve.evaluate(static_cast<float>(step) / 10.0F);
            EXPECT_GE(value, 0.0F);
            EXPECT_LE(value, 1.0F);
        }
    }
    EXPECT_NEAR((ResponseCurve{.shape = ResponseCurve::Shape::Logistic, .slope = 10.0F, .shift = 0.5F}).evaluate(0.5F), 0.5F, 1e-5F);
    EXPECT_NEAR((ResponseCurve{.shape = ResponseCurve::Shape::Logit, .slope = 1.0F}).evaluate(0.5F), 0.5F, 1e-5F);
    EXPECT_NEAR((ResponseCurve{.shape = ResponseCurve::Shape::Normal, .slope = 1.0F, .exponent = 10.0F, .shift = 0.5F}).evaluate(0.5F), 1.0F, 1e-5F);
    EXPECT_EQ(ResponseCurve::shapeFromName("logit"), ResponseCurve::Shape::Logit);
    EXPECT_EQ(ResponseCurve::shapeName(ResponseCurve::Shape::Normal), "normal");
}

TEST(InfluenceMapTest, StampsSpreadsAndFindsExtremes) {
    InfluenceMap threat(20, 10, 10.0F, {100.0F, 0.0F});
    threat.stamp({155.0F, 55.0F}, 1.0F, 30.0F);
    EXPECT_FLOAT_EQ(threat.get(5, 5), 1.0F);
    EXPECT_FLOAT_EQ(threat.get(0, 0), 0.0F);
    EXPECT_NEAR(threat.sample({155.0F, 55.0F}), 1.0F, 1e-5F);
    EXPECT_EQ(threat.sample({50.0F, 50.0F}), 0.0F);

    // Spreading carries the influence to cells the stamp never reached, fading with distance.
    threat.propagate(0.5F, 0.5F);
    threat.propagate(0.5F, 0.5F);
    EXPECT_GT(threat.get(5, 9), 0.0F);
    EXPECT_GT(threat.get(9, 5), 0.0F);
    EXPECT_GT(threat.get(6, 5), threat.get(9, 5));

    const std::optional<InfluenceMap::Spot> hot = threat.findHighest({150.0F, 50.0F}, 100.0F);
    ASSERT_TRUE(hot.has_value());
    EXPECT_LT(math::Vec2::distance(hot->position, {155.0F, 55.0F}), 15.0F);
    EXPECT_FLOAT_EQ(threat.findLowest({285.0F, 5.0F}, 1.0F)->value, threat.get(18, 0));
    EXPECT_FALSE(threat.findLowest({-500.0F, 0.0F}, 10.0F).has_value());

    InfluenceMap allies(20, 10, 10.0F, {100.0F, 0.0F});
    allies.fill(2.0F);
    allies.add(threat, -1.0F);
    EXPECT_FLOAT_EQ(allies.get(0, 0), 2.0F - threat.get(0, 0));
    allies.scale(0.5F);
    EXPECT_FLOAT_EQ(allies.get(0, 0), (2.0F - threat.get(0, 0)) * 0.5F);
    EXPECT_THROW(allies.add(InfluenceMap(2, 2, 1.0F)), std::invalid_argument);
    EXPECT_THROW((void)allies.get(20, 0), std::out_of_range);
    EXPECT_THROW(InfluenceMap(0, 1, 1.0F), std::invalid_argument);
    EXPECT_EQ(InfluenceMap::falloffFromName("quadratic"), InfluenceMap::Falloff::Quadratic);
}

} // namespace haylen::ai
