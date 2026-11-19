#include <gtest/gtest.h>

#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

#include "haylen/2d/animation/Animation.hpp"
#include "haylen/2d/animation/Animator.hpp"
#include "haylen/2d/graphics/Sprite.hpp"
#include "haylen/graphics/Device.hpp"
#include "haylen/graphics/Image.hpp"
#include "support/EngineFixture.hpp"

namespace haylen::animation2d {

class AnimationTest : public ::testing::Test {
  protected:
    [[nodiscard]] static Animation makeStrip(graphics::Texture texture, Animation::Loop loop) {
        return Animation::fromGrid(std::move(texture), {.frameSize = {16.0F, 16.0F}, .cells = {}, .framesPerSecond = 10.0F, .loop = loop});
    }
};

using AnimatorTest = AnimationTest;

TEST_F(AnimationTest, CutsGridsAndPicksFramesOverTime) {
    test::EngineFixture fixture;
    const graphics::Texture texture = fixture.engine().getGraphics().createTexture(graphics::Image(64, 32));

    const animation2d::Animation all = makeStrip(texture, animation2d::Animation::Loop::Loop);
    ASSERT_EQ(all.frames.size(), 8U);
    EXPECT_EQ(all.frames[5].source, (math::Rect{16.0F, 16.0F, 16.0F, 16.0F}));
    EXPECT_FLOAT_EQ(all.getDuration(), 0.8F);
    EXPECT_EQ(all.frameAt(0.0F), 0U);
    EXPECT_EQ(all.frameAt(0.25F), 2U);
    EXPECT_EQ(all.frameAt(0.85F), 0U);
    EXPECT_EQ(all.frameAt(-1.0F), 0U);

    const animation2d::Animation spaced = animation2d::Animation::fromGrid(texture, {.frameSize = {14.0F, 14.0F}, .cells = {1, 3}, .framesPerSecond = 5.0F, .loop = animation2d::Animation::Loop::Once, .margin = {1.0F, 1.0F}, .spacing = {2.0F, 2.0F}});
    ASSERT_EQ(spaced.frames.size(), 2U);
    EXPECT_EQ(spaced.frames[0].source, (math::Rect{17.0F, 1.0F, 14.0F, 14.0F}));
    EXPECT_EQ(spaced.frames[1].source, (math::Rect{49.0F, 1.0F, 14.0F, 14.0F}));
    EXPECT_EQ(spaced.frameAt(10.0F), 1U);

    animation2d::Animation bounce = animation2d::Animation::fromGrid(texture, {.frameSize = {16.0F, 16.0F}, .cells = {0, 1, 2, 3}, .framesPerSecond = 10.0F, .loop = animation2d::Animation::Loop::PingPong});
    std::vector<std::size_t> sequence;
    for (int step = 0; step < 8; ++step) {
        sequence.push_back(bounce.frameAt(static_cast<float>(step) * 0.1F + 0.05F));
    }

    EXPECT_EQ(sequence, (std::vector<std::size_t>{0, 1, 2, 3, 2, 1, 0, 1}));

    bounce.frames.resize(1);
    EXPECT_EQ(bounce.frameAt(5.0F), 0U);

    EXPECT_THROW((void)animation2d::Animation::fromGrid(graphics::Texture{}, {.frameSize = {16.0F, 16.0F}}), std::invalid_argument);
    EXPECT_THROW((void)animation2d::Animation::fromGrid(texture, {.frameSize = {128.0F, 16.0F}}), std::invalid_argument);
    EXPECT_THROW((void)animation2d::Animation::fromGrid(texture, {.frameSize = {16.0F, 16.0F}, .cells = {8}}), std::out_of_range);
    EXPECT_EQ(animation2d::Animation::loopFromName("pingPong"), animation2d::Animation::Loop::PingPong);
    EXPECT_FALSE(animation2d::Animation::loopFromName("forever").has_value());
    EXPECT_EQ(animation2d::Animation::loopName(animation2d::Animation::Loop::Once), "once");
}

// A time that rounding puts at the end of a pass still gets a later change, so an animator that walks the changes never stops at it.
TEST_F(AnimationTest, FindsALaterChangeAtTheRoundedEndOfAPass) {
    Animation animation;
    animation.frames.resize(2);

    for (SpriteFrame& frame : animation.frames) {
        frame.duration = 1.0F / 5.0F;
    }

    const float time = 84000 * 0.0001F;
    EXPECT_GT(animation.getNextChange(time).time, time);
}

TEST_F(AnimatorTest, PlaysAnimationsAndReportsProgress) {
    test::EngineFixture fixture;
    const graphics::Texture texture = fixture.engine().getGraphics().createTexture(graphics::Image(64, 16));

    animation2d::Animator animator;
    animator.add("run", makeStrip(texture, animation2d::Animation::Loop::Loop));
    animator.add("attack", makeStrip(texture, animation2d::Animation::Loop::Once));
    EXPECT_TRUE(animator.has("run"));
    EXPECT_FALSE(animator.isFinished());

    std::vector<std::string> events;
    animator.onFrame = [&](std::string_view name, std::size_t frame) { events.push_back(std::string(name) + ":" + std::to_string(frame)); };
    animator.onComplete = [&](std::string_view name) { events.push_back(std::string(name) + ":done"); };

    animator.update(1.0F);
    animator.play("run");
    animator.update(0.15F);
    EXPECT_EQ(animator.getFrame(), 1U);
    animator.play("run");
    EXPECT_EQ(animator.getFrame(), 1U);
    animator.play("run", true);
    EXPECT_EQ(animator.getFrame(), 0U);

    animator.play("attack");
    animator.speed = 2.0F;
    animator.update(0.1F);
    animator.update(1.0F);
    EXPECT_TRUE(animator.isFinished());
    EXPECT_FALSE(animator.isPlaying());
    EXPECT_EQ(events, (std::vector<std::string>{"run:1", "attack:1", "attack:2", "attack:3", "attack:done"}));

    graphics2d::Sprite sprite;
    animator.apply(sprite);
    EXPECT_EQ(sprite.texture, texture);
    EXPECT_EQ(sprite.source, (math::Rect{48.0F, 0.0F, 16.0F, 16.0F}));
    EXPECT_EQ(sprite.pivot, math::Vec2(0.5F, 0.5F));

    // Replacing the animation that plays finds its frame again among the new frames.
    animator.add("attack", animation2d::Animation::fromGrid(texture, {.frameSize = {16.0F, 16.0F}, .cells = {2}, .framesPerSecond = 10.0F, .loop = animation2d::Animation::Loop::Once}));
    EXPECT_EQ(animator.getFrame(), 0U);
    animator.apply(sprite);
    EXPECT_EQ(sprite.source, (math::Rect{32.0F, 0.0F, 16.0F, 16.0F}));

    animator.play("run");
    animator.stop();
    EXPECT_FALSE(animator.isPlaying());
    EXPECT_THROW(animator.play("fly"), std::invalid_argument);
    EXPECT_THROW(animator.add("", makeStrip(texture, animation2d::Animation::Loop::Loop)), std::invalid_argument);

    animation2d::Animator empty;
    graphics2d::Sprite untouched;
    empty.apply(untouched);
    EXPECT_FALSE(untouched.texture.isValid());
}

// A callback that replaces the animation that plays with a shorter one ends the step on the frame it was entering.
TEST_F(AnimatorTest, StopsTheStepWhenACallbackReplacesTheAnimation) {
    test::EngineFixture fixture;
    const graphics::Texture texture = fixture.engine().getGraphics().createTexture(graphics::Image(64, 16));
    animation2d::Animator animator;
    animator.add("run", makeStrip(texture, animation2d::Animation::Loop::Loop));
    animator.play("run");

    std::vector<std::string> events;
    // clang-format off
    animator.onFrame = [&](std::string_view name, std::size_t frame) {
        events.push_back(std::string(name) + ":" + std::to_string(frame));

        if (frame == 3) {
            animator.add("run", animation2d::Animation::fromGrid(texture, {.frameSize = {16.0F, 16.0F}, .cells = {0, 1}, .framesPerSecond = 100.0F}));
        }
    };
    // clang-format on
    animator.update(0.35F);
    EXPECT_EQ(events, (std::vector<std::string>{"run:1", "run:2", "run:3"}));
    EXPECT_LT(animator.getFrame(), 2U);
}

TEST_F(AnimatorTest, RefusesTimesThatAreNotFinite) {
    test::EngineFixture fixture;
    const graphics::Texture texture = fixture.engine().getGraphics().createTexture(graphics::Image(64, 16));
    animation2d::Animator animator;
    animator.add("run", makeStrip(texture, animation2d::Animation::Loop::Loop));
    animator.play("run");

    EXPECT_THROW(animator.update(std::numeric_limits<float>::infinity()), std::invalid_argument);
    EXPECT_THROW(animator.seek(std::numeric_limits<float>::infinity()), std::invalid_argument);
    animator.speed = std::numeric_limits<float>::infinity();
    EXPECT_THROW(animator.update(0.1F), std::invalid_argument);
    EXPECT_EQ(animator.getTime(), 0.0F);
}

TEST_F(AnimatorTest, QueuesAnimationsAfterTheCurrentPass) {
    test::EngineFixture fixture;
    const graphics::Texture texture = fixture.engine().getGraphics().createTexture(graphics::Image(64, 16));
    animation2d::Animator animator;
    animator.add("run", makeStrip(texture, animation2d::Animation::Loop::Loop));
    animator.add("attack", makeStrip(texture, animation2d::Animation::Loop::Once));
    animator.add("idle", makeStrip(texture, animation2d::Animation::Loop::PingPong));
    std::vector<std::string> finished;
    animator.onComplete = [&finished](std::string_view name) { finished.emplace_back(name); };

    animator.queue("attack");
    EXPECT_EQ(animator.getCurrent(), "attack");
    animator.queue("run");
    animator.queue("idle");
    EXPECT_EQ(animator.getQueuedCount(), 2U);

    animator.update(0.2F);
    EXPECT_EQ(animator.getCurrent(), "attack");
    animator.update(0.25F);
    EXPECT_EQ(finished, (std::vector<std::string>{"attack"}));
    EXPECT_EQ(animator.getCurrent(), "run");
    EXPECT_TRUE(animator.isPlaying());
    EXPECT_EQ(animator.getFrame(), 0U);
    EXPECT_EQ(animator.getQueuedCount(), 1U);

    animator.update(0.2F);
    EXPECT_EQ(animator.getCurrent(), "run");
    animator.update(0.25F);
    EXPECT_EQ(animator.getCurrent(), "idle");
    EXPECT_EQ(animator.getQueuedCount(), 0U);

    animator.queue("run");
    animator.play("attack");
    EXPECT_EQ(animator.getQueuedCount(), 0U);
    animator.queue("run");
    animator.clearQueue();
    EXPECT_EQ(animator.getQueuedCount(), 0U);
    EXPECT_THROW(animator.queue("fly"), std::invalid_argument);
}

TEST_F(AnimatorTest, QueuesAfterTheCycleInProgress) {
    test::EngineFixture fixture;
    const graphics::Texture texture = fixture.engine().getGraphics().createTexture(graphics::Image(64, 16));
    animation2d::Animator animator;
    animator.add("run", makeStrip(texture, animation2d::Animation::Loop::Loop));
    animator.add("idle", makeStrip(texture, animation2d::Animation::Loop::PingPong));
    animator.add("attack", makeStrip(texture, animation2d::Animation::Loop::Once));
    EXPECT_FLOAT_EQ(animator.getAnimation("run").getCycleDuration(), 0.4F);
    EXPECT_FLOAT_EQ(animator.getAnimation("idle").getCycleDuration(), 0.6F);
    EXPECT_FLOAT_EQ(animator.getAnimation("attack").getCycleDuration(), 0.4F);

    // The loop is inside its third cycle when the queue call comes, so the switch waits for that cycle to end.
    animator.play("run");
    animator.update(1.0F);
    animator.queue("idle");
    animator.update(0.1F);
    EXPECT_EQ(animator.getCurrent(), "run");
    animator.update(0.15F);
    EXPECT_EQ(animator.getCurrent(), "idle");

    // A ping-pong cycle ends when the animation is back at its first frame.
    animator.queue("attack");
    animator.update(0.45F);
    EXPECT_EQ(animator.getCurrent(), "idle");
    animator.update(0.2F);
    EXPECT_EQ(animator.getCurrent(), "attack");

    animator.update(1.0F);
    EXPECT_TRUE(animator.isFinished());
    animator.queue("run");
    EXPECT_EQ(animator.getCurrent(), "run");
    EXPECT_TRUE(animator.isPlaying());
}

TEST_F(AnimatorTest, QueuesAFinishedOneShotAnimationAgainFromItsStart) {
    test::EngineFixture fixture;
    const graphics::Texture texture = fixture.engine().getGraphics().createTexture(graphics::Image(64, 16));
    animation2d::Animator animator;
    animator.add("attack", makeStrip(texture, animation2d::Animation::Loop::Once));
    int finished = 0;
    animator.onComplete = [&finished](std::string_view) { ++finished; };

    animator.play("attack");
    animator.update(1.0F);
    ASSERT_TRUE(animator.isFinished());
    animator.queue("attack");
    EXPECT_TRUE(animator.isPlaying());
    EXPECT_FALSE(animator.isFinished());
    EXPECT_EQ(animator.getFrame(), 0U);
    animator.update(1.0F);
    EXPECT_EQ(finished, 2);
}

} // namespace haylen::animation2d
