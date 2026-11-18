#include <gtest/gtest.h>

#include <cmath>
#include <functional>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include "haylen/core/ConnectionScope.hpp"
#include "haylen/core/FrameClock.hpp"
#include "haylen/core/PropertyTrack.hpp"
#include "haylen/core/PropertyTween.hpp"
#include "haylen/core/Timeline.hpp"
#include "haylen/core/TweenManager.hpp"
#include "haylen/core/TweenMotion.hpp"
#include "haylen/core/TweenProperty.hpp"
#include "haylen/core/TweenValue.hpp"
#include "haylen/math/Math.hpp"
#include "support/AllocationTracker.hpp"
#include "support/EngineFixture.hpp"

namespace haylen::core {

struct Box {
    double x = 0.0;
    math::Vec2 position{};
};

// Plays tweens on a clock that accepts long frames, so a test can jump through time in one step.
class TweenRig final {
  public:
    void step(double seconds) {
        clock.advance(seconds);
        tweens.update(clock);
    }

    // Returns a tween of the `x` of the box, added to the manager unless told otherwise.
    std::shared_ptr<PropertyTween> tweenX(Box& box, float duration, TweenProperty property, bool add = true) {
        auto tween = std::make_shared<PropertyTween>(duration);
        tween->addTrack(numberTrack(box, std::move(property)));

        if (add) {
            tweens.add(tween);
        }

        return tween;
    }

    static std::unique_ptr<PropertyTrack> numberTrack(Box& box, TweenProperty property, std::function<bool()> alive = {}) {
        return std::make_unique<PropertyTrack>(&box, std::vector<std::string>{"x"}, [&box] { return TweenValue(box.x); }, [&box](const TweenValue& value) { box.x = value.getNumber(); }, std::move(property), std::move(alive));
    }

    FrameClock clock{1.0 / 60.0, 100.0};
    TweenManager tweens;
};

// A tween of two seconds that only remembers where it played to, the way a project extends the engine with its own kind of tween.
class PlayheadTween final : public Tween {
  public:
    [[nodiscard]] float getDuration() const noexcept override {
        return 2.0F;
    }

    float playhead = 0.0F;

  protected:
    void renderLoop(float loopTime, float, int, bool) override {
        playhead = loopTime;
    }
};

TEST(TweenValueTest, MixesEveryKind) {
    using Interpolation = TweenValue::Interpolation;
    EXPECT_DOUBLE_EQ(TweenValue::mix(0.0, 10.0, 0.25F, Interpolation::Linear).getNumber(), 2.5);
    EXPECT_DOUBLE_EQ(TweenValue::mix(0.0, 10.0, 0.26F, Interpolation::Integer).getNumber(), 3.0);

    // Angles take the shorter way, so 350 degrees to 10 degrees passes through 0.
    const double from = math::Math::radians(350.0F);
    const double to = math::Math::radians(10.0F);
    EXPECT_NEAR(TweenValue::mix(from, to, 0.5F, Interpolation::Angle).getNumber(), math::Math::radians(360.0F), 1e-4);
    EXPECT_NEAR(TweenValue::distance(from, to, Interpolation::Angle), math::Math::radians(20.0F), 1e-4);

    const math::Vec2 middle = TweenValue::mix(math::Vec2{0.0F, 0.0F}, math::Vec2{4.0F, 8.0F}, 0.5F, Interpolation::Linear).getVector();
    EXPECT_FLOAT_EQ(middle.x, 2.0F);
    EXPECT_FLOAT_EQ(middle.y, 4.0F);

    const math::Color red{1.0F, 0.0F, 0.0F, 1.0F};
    const math::Color blue{0.0F, 0.0F, 1.0F, 1.0F};
    const math::Color rgb = TweenValue::mix(red, blue, 0.5F, Interpolation::Linear).getColor();
    const math::Color hsv = TweenValue::mix(red, blue, 0.5F, Interpolation::Hsv).getColor();
    EXPECT_FLOAT_EQ(rgb.r, 0.5F);
    EXPECT_NEAR(hsv.r, 1.0F, 1e-4F);
    EXPECT_NEAR(hsv.b, 1.0F, 1e-4F);
    EXPECT_NEAR(hsv.g, 0.0F, 1e-4F);

    // A text types over the start text one character at a time.
    EXPECT_EQ(TweenValue::mix(std::string(), std::string("héllo"), 0.4F, Interpolation::Linear).getText(), "hé");
    EXPECT_EQ(TweenValue::mix(std::string("abcd"), std::string("wxyz"), 0.5F, Interpolation::Linear).getText(), "wxcd");
    EXPECT_FLOAT_EQ(TweenValue::distance(std::string(), std::string("héllo"), Interpolation::Linear), 5.0F);
    EXPECT_EQ(TweenValue::mix(std::string("abc"), std::string("xyz"), std::numeric_limits<float>::quiet_NaN(), Interpolation::Linear).getText(), "abc");

    EXPECT_DOUBLE_EQ(TweenValue::add(2.0, 3.0, 2.0F).getNumber(), 8.0);
    EXPECT_FLOAT_EQ(TweenValue::difference(math::Vec2{5.0F, 1.0F}, math::Vec2{2.0F, 1.0F}).getVector().x, 3.0F);
    EXPECT_EQ(TweenValue::add(math::Color{0.5F, 0.25F, 0.0F, 1.0F}, math::Color{0.25F, 0.25F, 0.5F, -0.5F}, 2.0F).getColor(), (math::Color{1.0F, 0.75F, 1.0F, 0.0F}));
    EXPECT_EQ(TweenValue::difference(blue, red).getColor(), (math::Color{-1.0F, 0.0F, 1.0F, 0.0F}));
    EXPECT_FLOAT_EQ(TweenValue::distance(math::Vec2{0.0F, 0.0F}, math::Vec2{3.0F, 4.0F}, Interpolation::Linear), 5.0F);
    EXPECT_FLOAT_EQ(TweenValue::distance(red, blue, Interpolation::Linear), 1.0F);
    EXPECT_THROW((void)TweenValue::mix(1.0, math::Vec2{}, 0.5F, Interpolation::Linear), std::invalid_argument);
    EXPECT_THROW((void)TweenValue::add(std::string("a"), std::string("b")), std::invalid_argument);
    EXPECT_THROW((void)TweenValue::difference(std::string("a"), std::string("b")), std::invalid_argument);

    // The end of a tween is its target exactly, even where subtracting and adding back the start rounds.
    EXPECT_EQ(TweenValue::mix(12.3, -4.56, 1.0F, Interpolation::Linear).getNumber(), -4.56);
    EXPECT_EQ(TweenValue::mix(math::Vec2{12.3F, 12.3F}, math::Vec2{-4.56F, -4.56F}, 1.0F, Interpolation::Linear).getVector(), (math::Vec2{-4.56F, -4.56F}));
    EXPECT_EQ(TweenValue::mix(math::Color{12.3F, 0.0F, 0.0F, 1.0F}, math::Color{-4.56F, 0.0F, 0.0F, 1.0F}, 1.0F, Interpolation::Linear).getColor().r, -4.56F);
}

TEST(TweenManagerTest, RejectsTimesThatAreNotNumbers) {
    TweenRig rig;
    Box box;
    const float nan = std::numeric_limits<float>::quiet_NaN();
    auto tween = rig.tweenX(box, 1.0F, TweenProperty::to(10.0));
    EXPECT_THROW(tween->setDelay(nan), std::invalid_argument);
    EXPECT_THROW(tween->setRepeatDelay(nan), std::invalid_argument);
    EXPECT_THROW(tween->setTimeScale(nan), std::invalid_argument);
    EXPECT_THROW(tween->seek(nan), std::invalid_argument);
    EXPECT_THROW(PropertyTween{nan}, std::invalid_argument);
    EXPECT_THROW(rig.tweens.setTimeScale("world", nan), std::invalid_argument);

    auto timeline = std::make_shared<Timeline>();
    EXPECT_THROW(timeline->insert(nan, rig.tweenX(box, 1.0F, TweenProperty::to(1.0), false)), std::invalid_argument);
    EXPECT_THROW(timeline->insertCall(nan, [] {}), std::invalid_argument);
    EXPECT_THROW(timeline->addLabel("lost", nan), std::invalid_argument);
    EXPECT_THROW(timeline->stagger({rig.tweenX(box, 1.0F, TweenProperty::to(1.0), false)}, nan), std::invalid_argument);
    EXPECT_THROW(timeline->stagger({rig.tweenX(box, 1.0F, TweenProperty::to(1.0), false)}, std::numeric_limits<float>::infinity()), std::invalid_argument);
    EXPECT_EQ(timeline->size(), 0U);
}

TEST(TweenPropertyTest, ResolvesModesWhenItBegins) {
    TweenProperty to = TweenProperty::to(10.0);
    to.begin(4.0);
    EXPECT_DOUBLE_EQ(to.evaluate(0.5F, 0).getNumber(), 7.0);

    TweenProperty from = TweenProperty::from(0.0);
    from.begin(8.0);
    EXPECT_DOUBLE_EQ(from.evaluate(0.25F, 0).getNumber(), 2.0);

    TweenProperty by = TweenProperty::by(5.0);
    by.begin(1.0);
    by.begin(100.0);
    EXPECT_DOUBLE_EQ(by.evaluate(1.0F, 0).getNumber(), 6.0);

    // Incremental loops move the whole range forward by one span per completed loop.
    EXPECT_DOUBLE_EQ(by.evaluate(0.0F, 2).getNumber(), 11.0);
    EXPECT_FLOAT_EQ(by.getDistance(), 5.0F);

    TweenProperty both = TweenProperty::fromTo(2.0, 4.0);
    both.begin(100.0);
    EXPECT_DOUBLE_EQ(both.evaluate(0.5F, 0).getNumber(), 3.0);
    EXPECT_EQ(both.getMode(), TweenProperty::Mode::FromTo);

    TweenProperty wrong = TweenProperty::to(math::Vec2{});
    EXPECT_THROW(wrong.begin(1.0), std::invalid_argument);
}

TEST(TweenMotionTest, FollowsShapesAndComesBack) {
    const TweenValue start = math::Vec2{0.0F, 0.0F};

    const auto jump = TweenMotion::jump(40.0F, 2);
    EXPECT_FLOAT_EQ(jump->evaluate(start, math::Vec2{100.0F, 0.0F}, 0.25F).getVector().y, -40.0F);
    EXPECT_NEAR(jump->evaluate(start, math::Vec2{100.0F, 0.0F}, 0.5F).getVector().y, 0.0F, 1e-4F);
    EXPECT_FLOAT_EQ(jump->evaluate(start, math::Vec2{100.0F, 0.0F}, 1.0F).getVector().x, 100.0F);

    // A straight path moves at constant speed through its corners.
    const auto path = TweenMotion::path({{10.0F, 0.0F}, {10.0F, 30.0F}}, false, false);
    path->prepare(start, math::Vec2{10.0F, 30.0F});
    EXPECT_FLOAT_EQ(path->getDistance(start, math::Vec2{}), 40.0F);
    EXPECT_EQ(path->evaluate(start, math::Vec2{}, 0.0F).getVector(), (math::Vec2{0.0F, 0.0F}));
    const math::Vec2 quarter = path->evaluate(start, math::Vec2{}, 0.25F).getVector();
    EXPECT_NEAR(quarter.x, 10.0F, 1e-3F);
    EXPECT_NEAR(quarter.y, 0.0F, 1e-3F);
    const auto heading = TweenMotion::orientation(path);
    EXPECT_NEAR(heading->evaluate(0.0, 0.0, 0.1F).getNumber(), 0.0, 1e-3);
    EXPECT_NEAR(heading->evaluate(0.0, 0.0, 0.8F).getNumber(), math::Math::kHalfPi, 1e-3);

    const auto curve = TweenMotion::path({{10.0F, 10.0F}, {20.0F, 0.0F}}, true, true);
    curve->prepare(start, math::Vec2{});
    EXPECT_NEAR(curve->evaluate(start, math::Vec2{}, 1.0F).getVector().x, 0.0F, 1e-3F);
    EXPECT_GT(curve->getDistance(start, math::Vec2{}), 40.0F);

    const auto bezier = TweenMotion::bezier({{0.0F, 100.0F}});
    EXPECT_FLOAT_EQ(bezier->evaluate(start, math::Vec2{100.0F, 0.0F}, 0.5F).getVector().y, 50.0F);
    const auto cubic = TweenMotion::bezier({{0.0F, 100.0F}, {100.0F, 100.0F}});
    EXPECT_FLOAT_EQ(cubic->evaluate(start, math::Vec2{100.0F, 0.0F}, 0.5F).getVector().y, 75.0F);
    EXPECT_GT(cubic->getDistance(start, math::Vec2{100.0F, 0.0F}), 100.0F);

    // Shakes and punches return to the start, and a seed makes a shake repeatable.
    const auto shake = TweenMotion::shake(10, 90.0F, 7U);
    const auto sameShake = TweenMotion::shake(10, 90.0F, 7U);
    const TweenValue strength = math::Vec2{5.0F, 5.0F};
    EXPECT_FLOAT_EQ(shake->evaluate(start, strength, 0.33F).getVector().x, sameShake->evaluate(start, strength, 0.33F).getVector().x);
    EXPECT_GT(shake->evaluate(start, strength, 0.1F).getVector().getLength(), 0.1F);
    EXPECT_NEAR(shake->evaluate(start, strength, 1.0F).getVector().getLength(), 0.0F, 1e-4F);
    EXPECT_NEAR(TweenMotion::shake(4, 0.0F, 1U)->evaluate(10.0, 2.0, 1.0F).getNumber(), 10.0, 1e-4);

    const auto punch = TweenMotion::punch(4, 0.5F);
    EXPECT_GT(punch->evaluate(0.0, 10.0, 0.1F).getNumber(), 0.0);
    EXPECT_NEAR(punch->evaluate(0.0, 10.0, 1.0F).getNumber(), 0.0, 1e-4);
    EXPECT_FLOAT_EQ(punch->getDistance(0.0, -10.0), 10.0F);

    const auto blink = TweenMotion::blink(2);
    EXPECT_DOUBLE_EQ(blink->evaluate(1.0, 0.0, 0.3F).getNumber(), 0.0);
    EXPECT_DOUBLE_EQ(blink->evaluate(1.0, 0.0, 0.6F).getNumber(), 1.0);
    EXPECT_DOUBLE_EQ(blink->evaluate(1.0, 0.0, 1.0F).getNumber(), 1.0);

    // The distance a speed-based tween travels is the strength of a shake, one unit per blink, nothing for a heading and the straight line of a jump.
    EXPECT_FLOAT_EQ(shake->getDistance(start, strength), strength.getVector().getLength());
    EXPECT_FLOAT_EQ(blink->getDistance(1.0, 0.0), 2.0F);
    EXPECT_FLOAT_EQ(heading->getDistance(0.0, 0.0), 0.0F);
    EXPECT_FLOAT_EQ(jump->getDistance(start, math::Vec2{30.0F, 40.0F}), 50.0F);

    EXPECT_THROW((void)TweenMotion::jump(1.0F, 0), std::invalid_argument);
    EXPECT_THROW((void)TweenMotion::shake(0, 90.0F, 1U), std::invalid_argument);
    EXPECT_THROW((void)TweenMotion::punch(0, 0.5F), std::invalid_argument);
    EXPECT_THROW((void)TweenMotion::blink(0), std::invalid_argument);
    EXPECT_THROW((void)TweenMotion::path({}, false, false), std::invalid_argument);
    EXPECT_THROW((void)TweenMotion::bezier({}), std::invalid_argument);
    EXPECT_THROW((void)TweenMotion::orientation(jump), std::invalid_argument);
    EXPECT_THROW(jump->prepare(1.0, 2.0), std::invalid_argument);
}

TEST(TweenManagerTest, PlaysEasedProgressWithDelayAndCallbacks) {
    TweenRig rig;
    Box box;
    std::vector<std::string> log;
    auto tween = rig.tweenX(box, 1.0F, TweenProperty::to(10.0));
    tween->setDelay(0.5F);
    tween->setEase(math::EasingCurve(math::Easing::Type::QuadIn));
    tween->setCallbacks({
        .start = [&] { log.emplace_back("start"); },
        .update = [&](float progress) { log.push_back(std::to_string(static_cast<int>(progress * 100.0F))); },
        .complete = [&] { log.emplace_back("complete"); },
        .kill = [&] { log.emplace_back("kill"); },
    });
    bool finished = false;
    tween->finished.connect([&](bool completed) { finished = completed; });

    rig.step(0.25);
    EXPECT_DOUBLE_EQ(box.x, 0.0);
    rig.step(0.75);
    EXPECT_DOUBLE_EQ(box.x, 2.5);
    EXPECT_TRUE(tween->isPlaying());
    rig.step(1.0);
    EXPECT_DOUBLE_EQ(box.x, 10.0);
    EXPECT_EQ(log, (std::vector<std::string>{"start", "25", "100", "complete", "kill"}));
    EXPECT_TRUE(finished);
    EXPECT_FALSE(tween->isAlive());
    EXPECT_EQ(rig.tweens.size(), 0U);
    EXPECT_THROW(rig.tweens.add(tween), std::logic_error);
    EXPECT_THROW(rig.tweens.add(nullptr), std::invalid_argument);
    EXPECT_THROW(PropertyTween(0.0F), std::invalid_argument);
    EXPECT_THROW(tween->addTrack(nullptr), std::invalid_argument);
    EXPECT_THROW(PropertyTrack(&box, {"x"}, {}, [](const TweenValue&) {}, TweenProperty::to(1.0)), std::invalid_argument);
    EXPECT_FLOAT_EQ(tween->getEase().apply(0.5F), 0.25F);
}

TEST(TweenManagerTest, ShortensOrSkipsTweensUnderReducedMotion) {
    TweenRig rig;
    Box kept;
    Box shortened;
    Box skipped;
    Box forever;
    rig.tweenX(kept, 1.0F, TweenProperty::to(10.0));
    rig.tweenX(shortened, 1.0F, TweenProperty::to(10.0))->setMotionReduction(MotionReduction::Shorten);
    bool completed = false;
    auto skipping = rig.tweenX(skipped, 1.0F, TweenProperty::to(10.0));
    skipping->setMotionReduction(MotionReduction::Skip);
    skipping->setCallbacks({.complete = [&completed] { completed = true; }});
    auto looping = rig.tweenX(forever, 1.0F, TweenProperty::to(10.0));
    looping->setRepeatCount(-1);
    looping->setMotionReduction(MotionReduction::Skip);

    // Without reduced motion every tween plays as it is.
    rig.step(0.125);
    EXPECT_DOUBLE_EQ(shortened.x, 1.25);
    EXPECT_DOUBLE_EQ(skipped.x, 1.25);

    rig.tweens.setReducedMotion(true);
    rig.step(0.125);
    EXPECT_DOUBLE_EQ(kept.x, 2.5);
    EXPECT_DOUBLE_EQ(shortened.x, 6.25);
    EXPECT_DOUBLE_EQ(skipped.x, 10.0);
    EXPECT_TRUE(completed);
    EXPECT_FALSE(skipping->isAlive());

    // A tween that repeats forever holds still until the system no longer asks for reduced motion.
    EXPECT_DOUBLE_EQ(forever.x, 1.25);
    rig.step(0.5);
    EXPECT_DOUBLE_EQ(forever.x, 1.25);
    rig.tweens.setReducedMotion(false);
    rig.step(0.125);
    EXPECT_DOUBLE_EQ(forever.x, 2.5);
}

TEST(TweenManagerTest, RepeatsWithEveryLoopMode) {
    TweenRig rig;
    Box restart;
    Box yoyo;
    Box incremental;
    std::vector<int> loops;
    auto first = rig.tweenX(restart, 1.0F, TweenProperty::to(10.0));
    first->setRepeatCount(2);
    first->setRepeatDelay(0.5F);
    first->getCallbacks().loop = [&](int loop) { loops.push_back(loop); };
    auto second = rig.tweenX(yoyo, 1.0F, TweenProperty::to(10.0));
    second->setRepeatCount(1);
    second->setLoopMode(Tween::LoopMode::Yoyo);
    auto third = rig.tweenX(incremental, 1.0F, TweenProperty::by(10.0));
    third->setRepeatCount(-1);
    third->setLoopMode(Tween::LoopMode::Incremental);
    EXPECT_TRUE(std::isinf(third->getTotalDuration()));
    EXPECT_FLOAT_EQ(first->getTotalDuration(), 4.0F);

    rig.step(1.25);
    EXPECT_DOUBLE_EQ(restart.x, 10.0);
    EXPECT_DOUBLE_EQ(yoyo.x, 7.5);
    EXPECT_DOUBLE_EQ(incremental.x, 12.5);
    rig.step(0.5);
    EXPECT_DOUBLE_EQ(restart.x, 2.5);
    EXPECT_DOUBLE_EQ(yoyo.x, 2.5);
    EXPECT_DOUBLE_EQ(incremental.x, 17.5);
    EXPECT_EQ(loops, std::vector<int>{1});
    EXPECT_NEAR(third->getProgress(), 0.75F, 1e-5F);

    rig.step(10.0);
    EXPECT_DOUBLE_EQ(restart.x, 10.0);
    EXPECT_DOUBLE_EQ(yoyo.x, 0.0);
    EXPECT_EQ(loops, (std::vector<int>{1, 2}));
    EXPECT_FALSE(first->isAlive());
    EXPECT_TRUE(third->isPlaying());
    third->complete();
    EXPECT_TRUE(third->isAlive());

    // Seeking a tween that repeats forever moves within its current loop, the twelfth one after 11.75 seconds.
    EXPECT_NEAR(incremental.x, 117.5, 1e-4);
    third->seek(0.25F);
    EXPECT_NEAR(incremental.x, 112.5, 1e-4);
    EXPECT_FLOAT_EQ(third->getTime(), 11.25F);
}

// A tween that repeats forever moves by every frame after days of play, and a step across countless loops reports only its last loops.
TEST(TweenManagerTest, RepeatsForeverWithoutLosingSteps) {
    TweenRig rig;
    Box box;
    auto tween = rig.tweenX(box, 0.1F, TweenProperty::to(1.0));
    tween->setRepeatCount(-1);
    int loops = 0;
    tween->getCallbacks().loop = [&loops](int) { ++loops; };
    rig.step(100.0);
    EXPECT_EQ(loops, 256);

    for (int step = 0; step < 12 * 864; ++step) {
        rig.step(100.0);
    }

    const float before = tween->getProgress();
    rig.step(0.01);
    EXPECT_NEAR(std::fmod(tween->getProgress() - before + 1.0F, 1.0F), 0.1F, 1e-3F);
}

// A tween that repeats as often as an int holds reports its last loop, the largest int, and ends where its last loop of a yoyo ends.
TEST(TweenManagerTest, RepeatsAsOftenAsAnIntHolds) {
    TweenRig rig;
    Box box;
    auto tween = rig.tweenX(box, 1.0F, TweenProperty::to(10.0));
    tween->setRepeatCount(std::numeric_limits<int>::max());
    tween->setLoopMode(Tween::LoopMode::Yoyo);
    std::vector<int> loops;
    tween->getCallbacks().loop = [&loops](int loop) { loops.push_back(loop); };
    EXPECT_FLOAT_EQ(tween->getTotalDuration(), 2147483648.0F);

    tween->complete();
    ASSERT_EQ(loops.size(), 256U);
    EXPECT_EQ(loops.back(), std::numeric_limits<int>::max());
    EXPECT_DOUBLE_EQ(box.x, 0.0);
}

TEST(TweenManagerTest, ControlsPlayback) {
    TweenRig rig;
    Box box;
    int completions = 0;
    auto tween = rig.tweenX(box, 2.0F, TweenProperty::to(20.0));
    tween->setAutoKill(false);
    tween->getCallbacks().complete = [&] { ++completions; };
    std::vector<bool> finished;
    tween->finished.connect([&](bool completed) { finished.push_back(completed); });

    rig.step(0.5);
    tween->pause();
    rig.step(5.0);
    EXPECT_DOUBLE_EQ(box.x, 5.0);
    EXPECT_FALSE(tween->isPlaying());
    tween->resume();
    tween->setTimeScale(2.0F);
    rig.step(0.5);
    EXPECT_DOUBLE_EQ(box.x, 15.0);

    tween->seek(0.5F);
    EXPECT_DOUBLE_EQ(box.x, 5.0);
    tween->setProgress(0.75F);
    EXPECT_DOUBLE_EQ(box.x, 15.0);
    EXPECT_FLOAT_EQ(tween->getTime(), 1.5F);

    // Reversing plays back to the start, where the reversed tween completes.
    tween->reverse();
    EXPECT_TRUE(tween->isReversed());
    rig.step(1.0);
    EXPECT_DOUBLE_EQ(box.x, 0.0);
    EXPECT_TRUE(tween->isCompleted());
    EXPECT_EQ(completions, 1);

    tween->play();
    tween->setTimeScale(1.0F);
    rig.step(3.0);
    EXPECT_DOUBLE_EQ(box.x, 20.0);
    EXPECT_EQ(completions, 2);
    EXPECT_TRUE(tween->isAlive());

    tween->restart();
    EXPECT_DOUBLE_EQ(box.x, 0.0);
    tween->complete(false);
    EXPECT_DOUBLE_EQ(box.x, 20.0);
    EXPECT_EQ(completions, 2);
    tween->kill();
    EXPECT_EQ(finished, (std::vector<bool>{true, true, true, false}));
    tween->kill();
    EXPECT_EQ(rig.tweens.size(), 0U);

    // A killed tween never moves its target again.
    tween->restart();
    tween->seek(0.5F);
    EXPECT_DOUBLE_EQ(box.x, 20.0);
    EXPECT_FALSE(tween->isAlive());
}

TEST(TweenManagerTest, CallbacksThatKillTheTweenStopItWhereItIs) {
    TweenRig rig;

    // A loop callback that kills the tween leaves the target where the loop ended, without rewinding it for the next loop.
    Box looping;
    auto tween = rig.tweenX(looping, 1.0F, TweenProperty::to(10.0));
    tween->setRepeatCount(2);
    tween->getCallbacks().loop = [&tween](int) { tween->kill(); };

    // A call at the end of a loop of a timeline that kills the timeline stops it there, before the next loop begins.
    Box timed;
    int loops = 0;
    auto timeline = std::make_shared<Timeline>();
    timeline->append(rig.tweenX(timed, 1.0F, TweenProperty::to(10.0), false));
    timeline->appendCall([&timeline] { timeline->kill(); });
    timeline->setRepeatCount(2);
    timeline->getCallbacks().loop = [&loops](int) { ++loops; };
    rig.tweens.add(timeline);

    // A complete callback that kills the tween ends it once.
    Box finishing;
    int kills = 0;
    auto completing = rig.tweenX(finishing, 1.0F, TweenProperty::to(10.0));
    completing->getCallbacks().complete = [&completing] { completing->kill(); };
    completing->getCallbacks().kill = [&kills] { ++kills; };

    // A loop callback that lets the target go stops the tween before it rewinds the target for the next loop.
    Box abandoned;
    bool alive = true;
    auto releasing = std::make_shared<PropertyTween>(1.0F);
    releasing->addTrack(TweenRig::numberTrack(abandoned, TweenProperty::to(10.0), [&alive] { return alive; }));
    releasing->setRepeatCount(2);
    releasing->getCallbacks().loop = [&alive](int) { alive = false; };
    rig.tweens.add(releasing);

    rig.step(1.5);
    EXPECT_DOUBLE_EQ(looping.x, 10.0);
    EXPECT_FALSE(tween->isAlive());
    EXPECT_DOUBLE_EQ(abandoned.x, 10.0);
    EXPECT_FALSE(releasing->isAlive());
    EXPECT_DOUBLE_EQ(timed.x, 10.0);
    EXPECT_EQ(loops, 0);
    EXPECT_FALSE(timeline->isAlive());
    EXPECT_DOUBLE_EQ(finishing.x, 10.0);
    EXPECT_EQ(kills, 1);
    EXPECT_FALSE(completing->isAlive());
    EXPECT_EQ(rig.tweens.size(), 0U);
}

// The finished signal fires and the tween ends even when its kill callback fails, so nothing waits for it forever.
TEST(TweenManagerTest, EndsEvenWhenTheKillCallbackFails) {
    TweenRig rig;
    Box box;
    auto tween = rig.tweenX(box, 1.0F, TweenProperty::to(10.0));
    tween->getCallbacks().kill = [] { throw std::runtime_error("the kill callback failed"); };
    std::vector<bool> finished;
    tween->finished.connect([&finished](bool completed) { finished.push_back(completed); });
    rig.step(0.5);

    EXPECT_THROW(tween->kill(), std::runtime_error);
    EXPECT_EQ(finished, std::vector<bool>{false});
    EXPECT_FALSE(tween->isAlive());
    rig.step(0.5);
    EXPECT_DOUBLE_EQ(box.x, 5.0);
    EXPECT_EQ(rig.tweens.size(), 0U);
}

// A tween of a project runs in the manager like the tweens of the engine, and kills by target and overwrites leave it alone, since it animates no target the engine knows.
TEST(TweenManagerTest, PlaysTweensThatProjectsDefine) {
    TweenRig rig;
    Box box;
    auto custom = std::make_shared<PlayheadTween>();
    std::vector<float> progress;
    custom->getCallbacks().update = [&progress](float value) { progress.push_back(value); };
    rig.tweens.add(custom);
    rig.step(0.5);

    rig.tweens.killTarget(&box);
    rig.tweens.overwrite(*rig.tweenX(box, 1.0F, TweenProperty::to(1.0), false));
    rig.step(0.5);
    EXPECT_TRUE(custom->isAlive());
    EXPECT_FLOAT_EQ(custom->playhead, 1.0F);
    EXPECT_EQ(progress, (std::vector<float>{0.25F, 0.5F}));
}

TEST(TweenManagerTest, KillingBeforeTheEndReportsNoCompletion) {
    TweenRig rig;
    Box box;
    auto tween = rig.tweenX(box, 1.0F, TweenProperty::to(1.0));
    bool completed = true;
    tween->finished.connect([&](bool value) { completed = value; });
    rig.step(0.5);
    Connection connection = tween->getConnection();
    EXPECT_TRUE(connection.isConnected());
    connection.setBlocked(true);
    EXPECT_TRUE(tween->isPaused());
    EXPECT_TRUE(connection.isBlocked());
    connection.disconnect();
    EXPECT_FALSE(completed);
    EXPECT_FALSE(connection.isConnected());
}

TEST(TweenManagerTest, RunsByProcessModeTimeKindAndGroup) {
    TweenRig rig;
    Box pausable;
    Box menu;
    Box unscaled;
    Box fixed;
    Box grouped;
    rig.tweenX(pausable, 1.0F, TweenProperty::to(1.0));
    rig.tweenX(menu, 1.0F, TweenProperty::to(1.0))->setProcessMode(ProcessMode::WhenPaused);
    auto real = rig.tweenX(unscaled, 1.0F, TweenProperty::to(1.0), false);
    real->setUnscaled(true);
    real->setProcessMode(ProcessMode::Always);
    rig.tweens.add(real);
    auto physics = rig.tweenX(fixed, 1.0F, TweenProperty::to(1.0), false);
    physics->setFixedStep(true);
    rig.tweens.add(physics);
    auto tagged = rig.tweenX(grouped, 1.0F, TweenProperty::to(1.0), false);
    tagged->setTag("world");
    rig.tweens.add(tagged);
    EXPECT_THROW(tagged->setTag("other"), std::logic_error);
    rig.tweens.setTimeScale("world", 0.5F);
    EXPECT_FLOAT_EQ(rig.tweens.getTimeScale("world"), 0.5F);
    EXPECT_FLOAT_EQ(rig.tweens.getTimeScale("missing"), 1.0F);

    rig.clock.setPaused(true);
    rig.clock.setTimeScale(0.5);
    rig.step(0.5);
    EXPECT_DOUBLE_EQ(pausable.x, 0.0);
    EXPECT_DOUBLE_EQ(menu.x, 0.25);
    EXPECT_DOUBLE_EQ(unscaled.x, 0.5);

    rig.clock.setPaused(false);
    rig.clock.setTimeScale(1.0);
    rig.step(0.5);
    EXPECT_DOUBLE_EQ(pausable.x, 0.5);
    EXPECT_DOUBLE_EQ(menu.x, 0.25);
    EXPECT_DOUBLE_EQ(grouped.x, 0.25);
    EXPECT_DOUBLE_EQ(fixed.x, 0.0);
    rig.tweens.fixedUpdate(rig.clock);
    EXPECT_NEAR(fixed.x, 1.0 / 60.0, 1e-6);
    EXPECT_THROW(rig.tweens.setTimeScale("world", -1.0F), std::invalid_argument);
}

// A tag keeps its time scale for the tweens that come later, while the tags that only tweens used leave no trace once those tweens end.
TEST(TweenManagerTest, ForgetsTagsThatNoTweenUses) {
    TweenRig rig;
    Box box;
    rig.tweens.setTimeScale("slow", 0.5F);
    // clang-format off
    const auto playTagged = [&rig, &box](const std::string& tag) {
        auto tween = rig.tweenX(box, 0.1F, TweenProperty::to(1.0), false);
        tween->setTag(tag);
        rig.tweens.add(tween);
        rig.step(0.2);
    };
    // clang-format on

    for (int index = 0; index < 4; ++index) {
        playTagged("a tag long enough to leave the buffer of a short text " + std::to_string(index));
    }

    const test::AllocationTracker tracker;
    for (int index = 4; index < 4000; ++index) {
        playTagged("a tag long enough to leave the buffer of a short text " + std::to_string(index));
    }

    EXPECT_LT(tracker.getLargest(), 4096U);
    EXPECT_FLOAT_EQ(rig.tweens.getTimeScale("slow"), 0.5F);

    box.x = 0.0;
    auto slow = rig.tweenX(box, 1.0F, TweenProperty::to(1.0), false);
    slow->setTag("slow");
    rig.tweens.add(slow);
    rig.step(0.5);
    EXPECT_DOUBLE_EQ(box.x, 0.25);
}

TEST(TweenManagerTest, InheritingTweensFollowTheModeOfTheirParent) {
    TweenRig rig;
    Box box;
    ProcessMode parent = ProcessMode::Pausable;
    const auto tween = rig.tweenX(box, 1.0F, TweenProperty::to(1.0));
    tween->setParentMode([&parent] { return parent; });
    EXPECT_EQ(tween->resolveProcessMode(), ProcessMode::Pausable);

    rig.clock.setPaused(true);
    rig.step(0.25);
    EXPECT_DOUBLE_EQ(box.x, 0.0);

    parent = ProcessMode::Always;
    rig.step(0.25);
    EXPECT_DOUBLE_EQ(box.x, 0.25);

    tween->setProcessMode(ProcessMode::Disabled);
    rig.step(0.25);
    EXPECT_DOUBLE_EQ(box.x, 0.25);
    EXPECT_EQ(tween->resolveProcessMode(), ProcessMode::Disabled);
}

TEST(TweenManagerTest, ActsOnTagsTargetsAndOverwrites) {
    TweenRig rig;
    Box first;
    Box second;
    auto a = rig.tweenX(first, 1.0F, TweenProperty::to(10.0), false);
    a->setTag("ui");
    rig.tweens.add(a);
    auto b = rig.tweenX(second, 1.0F, TweenProperty::to(10.0), false);
    b->setTag("ui");
    rig.tweens.add(b);

    rig.tweens.pauseTag("ui", true);
    rig.step(0.5);
    EXPECT_DOUBLE_EQ(first.x, 0.0);
    rig.tweens.pauseTag("ui", false);
    rig.tweens.completeTag("ui");
    EXPECT_DOUBLE_EQ(first.x, 10.0);
    EXPECT_DOUBLE_EQ(second.x, 10.0);
    EXPECT_EQ(rig.tweens.size(), 0U);

    auto old = rig.tweenX(first, 1.0F, TweenProperty::to(0.0));
    auto other = rig.tweenX(second, 1.0F, TweenProperty::to(0.0));
    auto newer = rig.tweenX(first, 1.0F, TweenProperty::to(50.0), false);
    rig.tweens.overwrite(*newer);
    rig.tweens.add(newer);
    EXPECT_FALSE(old->isAlive());
    EXPECT_TRUE(other->isAlive());

    rig.tweens.killTarget(&second);
    EXPECT_FALSE(other->isAlive());
    auto hud = rig.tweenX(second, 1.0F, TweenProperty::to(10.0), false);
    hud->setTag("hud");
    rig.tweens.add(hud);
    rig.tweens.killTag("missing");
    EXPECT_TRUE(hud->isAlive());
    rig.tweens.killTag("hud");
    EXPECT_FALSE(hud->isAlive());
    EXPECT_TRUE(newer->isAlive());
    rig.tweens.killAll();
    EXPECT_FALSE(newer->isAlive());
}

TEST(TweenManagerTest, StopsWhenTheTargetDies) {
    TweenRig rig;
    Box box;
    bool alive = true;
    auto tween = std::make_shared<PropertyTween>(1.0F);
    tween->addTrack(TweenRig::numberTrack(box, TweenProperty::to(10.0), [&alive] { return alive; }));
    rig.tweens.add(tween);
    rig.step(0.5);
    EXPECT_DOUBLE_EQ(box.x, 5.0);
    alive = false;
    rig.step(0.25);
    EXPECT_DOUBLE_EQ(box.x, 5.0);
    EXPECT_FALSE(tween->isAlive());
    EXPECT_THROW(tween->addTrack(TweenRig::numberTrack(box, TweenProperty::to(1.0))), std::logic_error);
}

TEST(TweenManagerTest, SpeedBasedTweensFindTheirDuration) {
    TweenRig rig;
    Box box;
    box.x = 10.0;
    auto tween = std::make_shared<PropertyTween>(20.0F, true);
    tween->addTrack(TweenRig::numberTrack(box, TweenProperty::to(50.0)));
    rig.tweens.add(tween);
    rig.step(1.0);
    EXPECT_FLOAT_EQ(tween->getDuration(), 2.0F);
    EXPECT_DOUBLE_EQ(box.x, 30.0);
}

TEST(TweenManagerTest, CallbacksMayChangeTweensWhileUpdating) {
    TweenRig rig;
    Box box;
    Box other;
    std::shared_ptr<PropertyTween> added;
    auto tween = rig.tweenX(box, 0.5F, TweenProperty::to(1.0));
    auto victim = rig.tweenX(other, 1.0F, TweenProperty::to(1.0));
    // clang-format off
    tween->getCallbacks().update = [&](float) {
        victim->kill();

        if (!added) {
            added = rig.tweenX(other, 1.0F, TweenProperty::to(-1.0));
        }
    };
    // clang-format on

    rig.step(0.25);
    EXPECT_FALSE(victim->isAlive());
    EXPECT_DOUBLE_EQ(other.x, 0.0);
    rig.step(0.5);
    EXPECT_DOUBLE_EQ(other.x, -0.5);

    auto self = rig.tweenX(box, 1.0F, TweenProperty::to(5.0));
    self->getCallbacks().start = [&] { self->kill(); };
    rig.step(0.5);
    EXPECT_FALSE(self->isAlive());

    // A tween that a callback moves into a timeline leaves the manager before the update reaches it.
    Box moved;
    auto taker = std::make_shared<Timeline>();
    auto trigger = rig.tweenX(box, 1.0F, TweenProperty::to(5.0));
    auto runner = rig.tweenX(moved, 1.0F, TweenProperty::to(5.0));
    trigger->getCallbacks().start = [&] { taker->append(runner); };
    rig.step(0.5);
    EXPECT_EQ(taker->size(), 1U);
    EXPECT_DOUBLE_EQ(moved.x, 0.0);
    EXPECT_EQ(rig.tweens.size(), 1U);
    EXPECT_FLOAT_EQ(taker->getDuration(), 1.0F);

    // Clearing drops tweens without running their callbacks.
    int calls = 0;
    rig.tweenX(box, 1.0F, TweenProperty::to(5.0))->getCallbacks().kill = [&] { ++calls; };
    rig.tweens.clear();
    EXPECT_EQ(calls, 0);
    EXPECT_EQ(rig.tweens.size(), 0U);
}

TEST(TimelineTest, PlacesStepsJoinsInsertsAndLabels) {
    TweenRig rig;
    Box first;
    Box second;
    std::vector<std::string> log;
    auto timeline = std::make_shared<Timeline>();
    timeline->append(rig.tweenX(first, 1.0F, TweenProperty::to(10.0), false));
    timeline->join(rig.tweenX(second, 0.5F, TweenProperty::to(5.0), false));
    timeline->appendCall([&] { log.push_back("call " + std::to_string(static_cast<int>(first.x))); });
    timeline->addLabel("pause");
    timeline->appendInterval(0.5F);
    timeline->append(rig.tweenX(first, 1.0F, TweenProperty::to(0.0), false));
    timeline->insertCall(0.25F, [&] { log.emplace_back("early"); });
    timeline->insert("pause", rig.tweenX(second, 0.5F, TweenProperty::to(-5.0), false));
    timeline->getCallbacks().step = [&](int step) { log.push_back("step " + std::to_string(step)); };
    EXPECT_FLOAT_EQ(timeline->getDuration(), 2.5F);
    EXPECT_FLOAT_EQ(timeline->getLabelTime("pause"), 1.0F);
    EXPECT_EQ(timeline->size(), 7U);
    rig.tweens.add(timeline);

    rig.step(0.5);
    EXPECT_DOUBLE_EQ(first.x, 5.0);
    EXPECT_DOUBLE_EQ(second.x, 5.0);
    rig.step(0.75);
    EXPECT_DOUBLE_EQ(first.x, 10.0);
    EXPECT_DOUBLE_EQ(second.x, 0.0);
    rig.step(0.75);
    EXPECT_DOUBLE_EQ(first.x, 5.0);
    rig.step(1.0);
    EXPECT_DOUBLE_EQ(first.x, 0.0);
    EXPECT_EQ(log, (std::vector<std::string>{"early", "call 10", "step 0", "step 1", "step 2", "step 3"}));
    EXPECT_FALSE(timeline->isAlive());
    EXPECT_THROW((void)timeline->getLabelTime("missing"), std::invalid_argument);
}

TEST(TimelineTest, SeeksAndReversesEverythingInside) {
    TweenRig rig;
    Box box;
    std::vector<std::string> calls;
    auto timeline = std::make_shared<Timeline>();
    timeline->append(rig.tweenX(box, 1.0F, TweenProperty::to(10.0), false));
    timeline->appendCall([&] { calls.emplace_back("middle"); });
    timeline->append(rig.tweenX(box, 1.0F, TweenProperty::to(20.0), false));
    timeline->addLabel("end");
    timeline->setAutoKill(false);
    rig.tweens.add(timeline);

    timeline->seek(1.5F);
    EXPECT_DOUBLE_EQ(box.x, 15.0);
    EXPECT_TRUE(calls.empty());
    timeline->seek(0.5F);
    EXPECT_DOUBLE_EQ(box.x, 5.0);
    timeline->seekLabel("end");
    EXPECT_DOUBLE_EQ(box.x, 20.0);

    timeline->reverse();
    rig.step(0.75);
    EXPECT_DOUBLE_EQ(box.x, 12.5);
    EXPECT_TRUE(calls.empty());
    rig.step(2.0);
    EXPECT_EQ(calls, std::vector<std::string>{"middle"});
    EXPECT_DOUBLE_EQ(box.x, 0.0);
    EXPECT_TRUE(timeline->isCompleted());
}

TEST(TimelineTest, RepeatsNestsAndStaggers) {
    TweenRig rig;
    Box outer;
    std::vector<Box> boxes(3);
    int calls = 0;
    auto inner = std::make_shared<Timeline>();
    inner->appendCall([&] { ++calls; });
    inner->append(rig.tweenX(outer, 1.0F, TweenProperty::fromTo(0.0, 10.0), false));
    auto timeline = std::make_shared<Timeline>();
    timeline->append(inner);
    timeline->setRepeatCount(2);
    rig.tweens.add(timeline);

    rig.step(0.5);
    EXPECT_DOUBLE_EQ(outer.x, 5.0);
    rig.step(1.0);
    EXPECT_DOUBLE_EQ(outer.x, 5.0);
    rig.step(5.0);
    EXPECT_DOUBLE_EQ(outer.x, 10.0);
    EXPECT_EQ(calls, 3);

    std::vector<std::shared_ptr<Tween>> tweens;
    for (Box& box : boxes) {
        tweens.push_back(rig.tweenX(box, 1.0F, TweenProperty::to(1.0), false));
    }

    auto staggered = std::make_shared<Timeline>();
    staggered->stagger(tweens, 0.5F, Timeline::StaggerOrigin::Center);
    rig.tweens.add(staggered);
    rig.step(0.5);
    EXPECT_DOUBLE_EQ(boxes[1].x, 0.5);
    EXPECT_DOUBLE_EQ(boxes[0].x, 0.0);
    EXPECT_DOUBLE_EQ(boxes[2].x, 0.0);
    EXPECT_FLOAT_EQ(staggered->getDuration(), 1.5F);

    auto ends = std::make_shared<Timeline>();
    ends->stagger({rig.tweenX(boxes[0], 1.0F, TweenProperty::to(2.0), false), rig.tweenX(boxes[1], 1.0F, TweenProperty::to(2.0), false)}, 1.0F, Timeline::StaggerOrigin::End);
    EXPECT_FLOAT_EQ(ends->getDuration(), 2.0F);
    EXPECT_THROW(ends->stagger({}, 1.0F), std::invalid_argument);
}

TEST(TimelineTest, RejectsInvalidChildrenAndKillsTheirTweens) {
    TweenRig rig;
    Box box;
    auto timeline = std::make_shared<Timeline>();
    auto started = rig.tweenX(box, 1.0F, TweenProperty::to(1.0));
    rig.step(0.1);
    EXPECT_THROW(timeline->append(started), std::logic_error);
    EXPECT_THROW(timeline->append(timeline), std::logic_error);
    EXPECT_THROW(timeline->append(nullptr), std::invalid_argument);
    EXPECT_THROW(timeline->appendCall({}), std::invalid_argument);
    EXPECT_THROW(timeline->addLabel(""), std::invalid_argument);
    auto speed = std::make_shared<PropertyTween>(10.0F, true);
    EXPECT_THROW(timeline->append(speed), std::logic_error);

    // A tween playing in the manager moves into the timeline that takes it.
    auto moved = rig.tweenX(box, 1.0F, TweenProperty::to(2.0));
    auto killed = rig.tweenX(box, 1.0F, TweenProperty::to(3.0), false);
    timeline->append(moved).append(killed);
    EXPECT_EQ(rig.tweens.size(), 1U);
    int kills = 0;
    moved->getCallbacks().kill = [&] { ++kills; };
    killed->getCallbacks().kill = [&] { ++kills; };
    killed->kill();
    EXPECT_EQ(timeline->size(), 1U);
    EXPECT_FLOAT_EQ(timeline->getDuration(), 1.0F);

    rig.tweens.add(timeline);
    rig.tweens.killTarget(&box);
    EXPECT_EQ(kills, 2);
    EXPECT_TRUE(timeline->isAlive());
    timeline->kill();
    EXPECT_EQ(timeline->size(), 0U);
}

TEST(TimelineTest, AStaggerThatFailsLeavesItsTweensWhereTheyWere) {
    TweenRig rig;
    Box box;
    auto playing = rig.tweenX(box, 1.0F, TweenProperty::to(10.0));
    auto staggered = std::make_shared<Timeline>();
    EXPECT_THROW(staggered->stagger({playing, playing}, 0.1F), std::logic_error);
    EXPECT_EQ(staggered->size(), 0U);
    staggered.reset();

    rig.step(0.5);
    EXPECT_DOUBLE_EQ(box.x, 5.0);
    playing->kill();
    EXPECT_FALSE(playing->isAlive());
}

// A tween that outlives the timeline that held it belongs to no timeline anymore, and a killed timeline lets go of the timelines inside it.
TEST(TimelineTest, TweensOutliveTheirTimeline) {
    TweenRig rig;
    Box box;
    auto orphan = rig.tweenX(box, 1.0F, TweenProperty::to(1.0), false);
    auto first = std::make_shared<Timeline>();
    first->append(orphan);
    first.reset();
    auto adopting = std::make_shared<Timeline>();
    adopting->append(orphan);
    EXPECT_EQ(adopting->size(), 1U);

    auto outer = std::make_shared<Timeline>();
    auto inner = std::make_shared<Timeline>();
    outer->append(inner);
    outer->kill();
    outer.reset();
    inner->append(rig.tweenX(box, 1.0F, TweenProperty::to(2.0), false));
    EXPECT_EQ(inner->size(), 1U);
}

TEST(TimelineTest, JoinsIntoTheFirstStepAndPlaysCallsAtOnce) {
    TweenRig rig;
    Box first;
    Box second;

    // Joining into an empty timeline starts its first step, and the update callback receives the progress through the loop.
    auto parallel = std::make_shared<Timeline>();
    parallel->join(rig.tweenX(first, 1.0F, TweenProperty::to(10.0), false));
    parallel->join(rig.tweenX(second, 0.5F, TweenProperty::to(10.0), false));
    float parallelProgress = 0.0F;
    parallel->getCallbacks().update = [&parallelProgress](float value) { parallelProgress = value; };
    EXPECT_FLOAT_EQ(parallel->getDuration(), 1.0F);
    rig.tweens.add(parallel);
    rig.step(0.5);
    EXPECT_DOUBLE_EQ(first.x, 5.0);
    EXPECT_DOUBLE_EQ(second.x, 10.0);
    EXPECT_FLOAT_EQ(parallelProgress, 0.5F);

    // A timeline of calls alone takes no time, so it runs every call and step and completes at once.
    std::vector<std::string> log;
    float callsProgress = 0.0F;
    auto calls = std::make_shared<Timeline>();
    calls->joinCall([&log] { log.emplace_back("first"); });
    calls->joinCall([&log] { log.emplace_back("joined"); });
    calls->appendCall([&log] { log.emplace_back("next"); });
    calls->getCallbacks().step = [&log](int step) { log.push_back("step " + std::to_string(step)); };
    calls->getCallbacks().update = [&callsProgress](float value) { callsProgress = value; };
    EXPECT_FLOAT_EQ(calls->getDuration(), 0.0F);
    rig.tweens.add(calls);
    rig.step(0.1);
    EXPECT_EQ(log, (std::vector<std::string>{"first", "joined", "next", "step 0", "step 1"}));
    EXPECT_FLOAT_EQ(callsProgress, 1.0F);
    EXPECT_TRUE(calls->isCompleted());
    EXPECT_FALSE(calls->isAlive());
}

TEST(TimelineTest, CallsMayChangeTheTimelineWhileItPlays) {
    TweenRig rig;
    Box first;
    Box second;
    std::vector<std::string> log;
    float duringCall = 0.0F;
    auto timeline = std::make_shared<Timeline>();
    auto later = rig.tweenX(second, 3.0F, TweenProperty::to(30.0), false);
    timeline->append(rig.tweenX(first, 1.0F, TweenProperty::to(10.0), false));

    // A call kills a tween that comes after it in the same render and inserts another call, and the timeline counts only what it still holds.
    // clang-format off
    timeline->insertCall(0.5F, [&] {
        later->kill();
        timeline->insertCall(0.75F, [&log] { log.emplace_back("inserted"); });
        duringCall = timeline->getDuration();
    });
    // clang-format on

    timeline->insert(0.5F, later);
    EXPECT_FLOAT_EQ(timeline->getDuration(), 3.5F);
    rig.tweens.add(timeline);

    rig.step(0.6);
    EXPECT_FLOAT_EQ(duringCall, 1.0F);
    EXPECT_FALSE(later->isAlive());
    EXPECT_DOUBLE_EQ(second.x, 0.0);
    EXPECT_EQ(timeline->size(), 3U);
    rig.step(0.2);
    EXPECT_EQ(log, std::vector<std::string>{"inserted"});
    rig.step(0.5);
    EXPECT_DOUBLE_EQ(first.x, 10.0);
    EXPECT_FALSE(timeline->isAlive());
}

TEST(TimelineTest, TweensInsideGiveUpTheFieldsThatANewerTweenOverwrites) {
    TweenRig rig;
    Box box;
    Box other;
    auto timeline = std::make_shared<Timeline>();
    auto inside = rig.tweenX(box, 1.0F, TweenProperty::to(10.0), false);
    timeline->append(inside).join(rig.tweenX(other, 1.0F, TweenProperty::to(10.0), false));
    rig.tweens.add(timeline);
    rig.step(0.5);

    auto newer = rig.tweenX(box, 1.0F, TweenProperty::to(-10.0), false);
    rig.tweens.overwrite(*newer);
    rig.tweens.add(newer);
    EXPECT_FALSE(inside->isAlive());
    EXPECT_TRUE(timeline->isAlive());
    EXPECT_EQ(timeline->size(), 1U);

    // A tween that overwrites again from the manager keeps its own fields.
    rig.tweens.overwrite(*newer);
    EXPECT_TRUE(newer->isAlive());
    rig.step(0.5);
    EXPECT_DOUBLE_EQ(box.x, -2.5);
    EXPECT_DOUBLE_EQ(other.x, 10.0);
}

TEST(TweenManagerTest, OwnerScopesKillTweens) {
    TweenRig rig;
    Box box;
    auto tween = rig.tweenX(box, 1.0F, TweenProperty::to(1.0));
    {
        ConnectionScope scope;
        scope.add(tween->getConnection());
    }

    EXPECT_FALSE(tween->isAlive());
}

} // namespace haylen::core
