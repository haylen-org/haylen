#include <gtest/gtest.h>

#include <cmath>
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
#include "support/EngineFixture.hpp"

namespace haylen::core {

namespace {

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

    // Returns a tween of the x of the box, added to the manager unless told otherwise.
    std::shared_ptr<core::PropertyTween> tweenX(Box& box, float duration, core::TweenProperty property, bool add = true) {
        auto tween = std::make_shared<core::PropertyTween>(duration);
        tween->addTrack(numberTrack(box, std::move(property)));
        if (add) {
            tweens.add(tween);
        }
        return tween;
    }

    static std::unique_ptr<core::PropertyTrack> numberTrack(Box& box, core::TweenProperty property, std::function<bool()> alive = {}) {
        return std::make_unique<core::PropertyTrack>(&box, std::vector<std::string>{"x"}, [&box] { return core::TweenValue(box.x); }, [&box](const core::TweenValue& value) { box.x = value.getNumber(); }, std::move(property), std::move(alive));
    }

    core::FrameClock clock{1.0 / 60.0, 100.0};
    core::TweenManager tweens;
};

} // namespace

TEST(TweenValueTest, MixesEveryKind) {
    using Interpolation = core::TweenValue::Interpolation;
    EXPECT_DOUBLE_EQ(core::TweenValue::mix(0.0, 10.0, 0.25F, Interpolation::Linear).getNumber(), 2.5);
    EXPECT_DOUBLE_EQ(core::TweenValue::mix(0.0, 10.0, 0.26F, Interpolation::Integer).getNumber(), 3.0);

    // Angles take the shorter way, so 350 degrees to 10 degrees passes through 0.
    const double from = math::Math::radians(350.0F);
    const double to = math::Math::radians(10.0F);
    EXPECT_NEAR(core::TweenValue::mix(from, to, 0.5F, Interpolation::Angle).getNumber(), math::Math::radians(360.0F), 1e-4);
    EXPECT_NEAR(core::TweenValue::distance(from, to, Interpolation::Angle), math::Math::radians(20.0F), 1e-4);

    const math::Vec2 middle = core::TweenValue::mix(math::Vec2{0.0F, 0.0F}, math::Vec2{4.0F, 8.0F}, 0.5F, Interpolation::Linear).getVector();
    EXPECT_FLOAT_EQ(middle.x, 2.0F);
    EXPECT_FLOAT_EQ(middle.y, 4.0F);

    const math::Color red{1.0F, 0.0F, 0.0F, 1.0F};
    const math::Color blue{0.0F, 0.0F, 1.0F, 1.0F};
    const math::Color rgb = core::TweenValue::mix(red, blue, 0.5F, Interpolation::Linear).getColor();
    const math::Color hsv = core::TweenValue::mix(red, blue, 0.5F, Interpolation::Hsv).getColor();
    EXPECT_FLOAT_EQ(rgb.r, 0.5F);
    EXPECT_NEAR(hsv.r, 1.0F, 1e-4F);
    EXPECT_NEAR(hsv.b, 1.0F, 1e-4F);
    EXPECT_NEAR(hsv.g, 0.0F, 1e-4F);

    // A text types over the start text one character at a time.
    EXPECT_EQ(core::TweenValue::mix(std::string(), std::string("héllo"), 0.4F, Interpolation::Linear).getText(), "hé");
    EXPECT_EQ(core::TweenValue::mix(std::string("abcd"), std::string("wxyz"), 0.5F, Interpolation::Linear).getText(), "wxcd");
    EXPECT_FLOAT_EQ(core::TweenValue::distance(std::string(), std::string("héllo"), Interpolation::Linear), 5.0F);

    EXPECT_DOUBLE_EQ(core::TweenValue::add(2.0, 3.0, 2.0F).getNumber(), 8.0);
    EXPECT_FLOAT_EQ(core::TweenValue::difference(math::Vec2{5.0F, 1.0F}, math::Vec2{2.0F, 1.0F}).getVector().x, 3.0F);
    EXPECT_FLOAT_EQ(core::TweenValue::distance(math::Vec2{0.0F, 0.0F}, math::Vec2{3.0F, 4.0F}, Interpolation::Linear), 5.0F);
    EXPECT_FLOAT_EQ(core::TweenValue::distance(red, blue, Interpolation::Linear), 1.0F);
    EXPECT_THROW((void)core::TweenValue::mix(1.0, math::Vec2{}, 0.5F, Interpolation::Linear), std::invalid_argument);
    EXPECT_THROW((void)core::TweenValue::add(std::string("a"), std::string("b")), std::invalid_argument);

    // The end of a tween is its target exactly, even where subtracting and adding back the start rounds.
    EXPECT_EQ(core::TweenValue::mix(12.3, -4.56, 1.0F, Interpolation::Linear).getNumber(), -4.56);
    EXPECT_EQ(core::TweenValue::mix(math::Vec2{12.3F, 12.3F}, math::Vec2{-4.56F, -4.56F}, 1.0F, Interpolation::Linear).getVector(), (math::Vec2{-4.56F, -4.56F}));
    EXPECT_EQ(core::TweenValue::mix(math::Color{12.3F, 0.0F, 0.0F, 1.0F}, math::Color{-4.56F, 0.0F, 0.0F, 1.0F}, 1.0F, Interpolation::Linear).getColor().r, -4.56F);
}

TEST(TweenManagerTest, RejectsTimesThatAreNotNumbers) {
    TweenRig rig;
    Box box;
    const float nan = std::numeric_limits<float>::quiet_NaN();
    auto tween = rig.tweenX(box, 1.0F, core::TweenProperty::to(10.0));
    EXPECT_THROW(tween->setDelay(nan), std::invalid_argument);
    EXPECT_THROW(tween->setRepeatDelay(nan), std::invalid_argument);
    EXPECT_THROW(tween->setTimeScale(nan), std::invalid_argument);
    EXPECT_THROW(tween->seek(nan), std::invalid_argument);
    EXPECT_THROW(core::PropertyTween{nan}, std::invalid_argument);
    EXPECT_THROW(rig.tweens.setTimeScale("world", nan), std::invalid_argument);
}

TEST(TweenPropertyTest, ResolvesModesWhenItBegins) {
    core::TweenProperty to = core::TweenProperty::to(10.0);
    to.begin(4.0);
    EXPECT_DOUBLE_EQ(to.evaluate(0.5F, 0).getNumber(), 7.0);

    core::TweenProperty from = core::TweenProperty::from(0.0);
    from.begin(8.0);
    EXPECT_DOUBLE_EQ(from.evaluate(0.25F, 0).getNumber(), 2.0);

    core::TweenProperty by = core::TweenProperty::by(5.0);
    by.begin(1.0);
    by.begin(100.0);
    EXPECT_DOUBLE_EQ(by.evaluate(1.0F, 0).getNumber(), 6.0);

    // Incremental loops move the whole range forward by one span per completed loop.
    EXPECT_DOUBLE_EQ(by.evaluate(0.0F, 2).getNumber(), 11.0);
    EXPECT_FLOAT_EQ(by.getDistance(), 5.0F);

    core::TweenProperty both = core::TweenProperty::fromTo(2.0, 4.0);
    both.begin(100.0);
    EXPECT_DOUBLE_EQ(both.evaluate(0.5F, 0).getNumber(), 3.0);
    EXPECT_EQ(both.getMode(), core::TweenProperty::Mode::FromTo);

    core::TweenProperty wrong = core::TweenProperty::to(math::Vec2{});
    EXPECT_THROW(wrong.begin(1.0), std::invalid_argument);
}

TEST(TweenMotionTest, FollowsShapesAndComesBack) {
    const core::TweenValue start = math::Vec2{0.0F, 0.0F};

    const auto jump = core::TweenMotion::jump(40.0F, 2);
    EXPECT_FLOAT_EQ(jump->evaluate(start, math::Vec2{100.0F, 0.0F}, 0.25F).getVector().y, -40.0F);
    EXPECT_NEAR(jump->evaluate(start, math::Vec2{100.0F, 0.0F}, 0.5F).getVector().y, 0.0F, 1e-4F);
    EXPECT_FLOAT_EQ(jump->evaluate(start, math::Vec2{100.0F, 0.0F}, 1.0F).getVector().x, 100.0F);

    // A straight path moves at constant speed through its corners.
    const auto path = core::TweenMotion::path({{10.0F, 0.0F}, {10.0F, 30.0F}}, false, false);
    path->prepare(start, math::Vec2{10.0F, 30.0F});
    EXPECT_FLOAT_EQ(path->getDistance(start, math::Vec2{}), 40.0F);
    const math::Vec2 quarter = path->evaluate(start, math::Vec2{}, 0.25F).getVector();
    EXPECT_NEAR(quarter.x, 10.0F, 1e-3F);
    EXPECT_NEAR(quarter.y, 0.0F, 1e-3F);
    const auto heading = core::TweenMotion::orientation(path);
    EXPECT_NEAR(heading->evaluate(0.0, 0.0, 0.1F).getNumber(), 0.0, 1e-3);
    EXPECT_NEAR(heading->evaluate(0.0, 0.0, 0.8F).getNumber(), math::Math::kHalfPi, 1e-3);

    const auto curve = core::TweenMotion::path({{10.0F, 10.0F}, {20.0F, 0.0F}}, true, true);
    curve->prepare(start, math::Vec2{});
    EXPECT_NEAR(curve->evaluate(start, math::Vec2{}, 1.0F).getVector().x, 0.0F, 1e-3F);
    EXPECT_GT(curve->getDistance(start, math::Vec2{}), 40.0F);

    const auto bezier = core::TweenMotion::bezier({{0.0F, 100.0F}});
    EXPECT_FLOAT_EQ(bezier->evaluate(start, math::Vec2{100.0F, 0.0F}, 0.5F).getVector().y, 50.0F);
    const auto cubic = core::TweenMotion::bezier({{0.0F, 100.0F}, {100.0F, 100.0F}});
    EXPECT_FLOAT_EQ(cubic->evaluate(start, math::Vec2{100.0F, 0.0F}, 0.5F).getVector().y, 75.0F);
    EXPECT_GT(cubic->getDistance(start, math::Vec2{100.0F, 0.0F}), 100.0F);

    // Shakes and punches return to the start, and a seed makes a shake repeatable.
    const auto shake = core::TweenMotion::shake(10, 90.0F, 7U);
    const auto sameShake = core::TweenMotion::shake(10, 90.0F, 7U);
    const core::TweenValue strength = math::Vec2{5.0F, 5.0F};
    EXPECT_FLOAT_EQ(shake->evaluate(start, strength, 0.33F).getVector().x, sameShake->evaluate(start, strength, 0.33F).getVector().x);
    EXPECT_GT(shake->evaluate(start, strength, 0.1F).getVector().getLength(), 0.1F);
    EXPECT_NEAR(shake->evaluate(start, strength, 1.0F).getVector().getLength(), 0.0F, 1e-4F);
    EXPECT_NEAR(core::TweenMotion::shake(4, 0.0F, 1U)->evaluate(10.0, 2.0, 1.0F).getNumber(), 10.0, 1e-4);

    const auto punch = core::TweenMotion::punch(4, 0.5F);
    EXPECT_GT(punch->evaluate(0.0, 10.0, 0.1F).getNumber(), 0.0);
    EXPECT_NEAR(punch->evaluate(0.0, 10.0, 1.0F).getNumber(), 0.0, 1e-4);
    EXPECT_FLOAT_EQ(punch->getDistance(0.0, -10.0), 10.0F);

    const auto blink = core::TweenMotion::blink(2);
    EXPECT_DOUBLE_EQ(blink->evaluate(1.0, 0.0, 0.3F).getNumber(), 0.0);
    EXPECT_DOUBLE_EQ(blink->evaluate(1.0, 0.0, 0.6F).getNumber(), 1.0);
    EXPECT_DOUBLE_EQ(blink->evaluate(1.0, 0.0, 1.0F).getNumber(), 1.0);

    EXPECT_THROW((void)core::TweenMotion::jump(1.0F, 0), std::invalid_argument);
    EXPECT_THROW((void)core::TweenMotion::path({}, false, false), std::invalid_argument);
    EXPECT_THROW((void)core::TweenMotion::bezier({}), std::invalid_argument);
    EXPECT_THROW((void)core::TweenMotion::orientation(jump), std::invalid_argument);
    EXPECT_THROW(jump->prepare(1.0, 2.0), std::invalid_argument);
}

TEST(TweenManagerTest, PlaysEasedProgressWithDelayAndCallbacks) {
    TweenRig rig;
    Box box;
    std::vector<std::string> log;
    auto tween = rig.tweenX(box, 1.0F, core::TweenProperty::to(10.0));
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
    EXPECT_THROW(core::PropertyTween(0.0F), std::invalid_argument);
}

TEST(TweenManagerTest, RepeatsWithEveryLoopMode) {
    TweenRig rig;
    Box restart;
    Box yoyo;
    Box incremental;
    std::vector<int> loops;
    auto first = rig.tweenX(restart, 1.0F, core::TweenProperty::to(10.0));
    first->setRepeatCount(2);
    first->setRepeatDelay(0.5F);
    first->getCallbacks().loop = [&](int loop) { loops.push_back(loop); };
    auto second = rig.tweenX(yoyo, 1.0F, core::TweenProperty::to(10.0));
    second->setRepeatCount(1);
    second->setLoopMode(core::Tween::LoopMode::Yoyo);
    auto third = rig.tweenX(incremental, 1.0F, core::TweenProperty::by(10.0));
    third->setRepeatCount(-1);
    third->setLoopMode(core::Tween::LoopMode::Incremental);
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
}

TEST(TweenManagerTest, ControlsPlayback) {
    TweenRig rig;
    Box box;
    int completions = 0;
    auto tween = rig.tweenX(box, 2.0F, core::TweenProperty::to(20.0));
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
}

TEST(TweenManagerTest, KillingBeforeTheEndReportsNoCompletion) {
    TweenRig rig;
    Box box;
    auto tween = rig.tweenX(box, 1.0F, core::TweenProperty::to(1.0));
    bool completed = true;
    tween->finished.connect([&](bool value) { completed = value; });
    rig.step(0.5);
    core::Connection connection = tween->getConnection();
    EXPECT_TRUE(connection.isConnected());
    connection.setBlocked(true);
    EXPECT_TRUE(tween->isPaused());
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
    rig.tweenX(pausable, 1.0F, core::TweenProperty::to(1.0));
    rig.tweenX(menu, 1.0F, core::TweenProperty::to(1.0))->setProcessMode(core::ProcessMode::WhenPaused);
    auto real = rig.tweenX(unscaled, 1.0F, core::TweenProperty::to(1.0), false);
    real->setUnscaled(true);
    real->setProcessMode(core::ProcessMode::Always);
    rig.tweens.add(real);
    auto physics = rig.tweenX(fixed, 1.0F, core::TweenProperty::to(1.0), false);
    physics->setFixedStep(true);
    rig.tweens.add(physics);
    auto tagged = rig.tweenX(grouped, 1.0F, core::TweenProperty::to(1.0), false);
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

TEST(TweenManagerTest, InheritingTweensFollowTheModeOfTheirParent) {
    TweenRig rig;
    Box box;
    core::ProcessMode parent = core::ProcessMode::Pausable;
    const auto tween = rig.tweenX(box, 1.0F, core::TweenProperty::to(1.0));
    tween->setParentMode([&parent] { return parent; });
    EXPECT_EQ(tween->resolveProcessMode(), core::ProcessMode::Pausable);

    rig.clock.setPaused(true);
    rig.step(0.25);
    EXPECT_DOUBLE_EQ(box.x, 0.0);

    parent = core::ProcessMode::Always;
    rig.step(0.25);
    EXPECT_DOUBLE_EQ(box.x, 0.25);

    tween->setProcessMode(core::ProcessMode::Disabled);
    rig.step(0.25);
    EXPECT_DOUBLE_EQ(box.x, 0.25);
    EXPECT_EQ(tween->resolveProcessMode(), core::ProcessMode::Disabled);
}

TEST(TweenManagerTest, ActsOnTagsTargetsAndOverwrites) {
    TweenRig rig;
    Box first;
    Box second;
    auto a = rig.tweenX(first, 1.0F, core::TweenProperty::to(10.0), false);
    a->setTag("ui");
    rig.tweens.add(a);
    auto b = rig.tweenX(second, 1.0F, core::TweenProperty::to(10.0), false);
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

    auto old = rig.tweenX(first, 1.0F, core::TweenProperty::to(0.0));
    auto other = rig.tweenX(second, 1.0F, core::TweenProperty::to(0.0));
    auto newer = rig.tweenX(first, 1.0F, core::TweenProperty::to(50.0), false);
    rig.tweens.overwrite(*newer);
    rig.tweens.add(newer);
    EXPECT_FALSE(old->isAlive());
    EXPECT_TRUE(other->isAlive());

    rig.tweens.killTarget(&second);
    EXPECT_FALSE(other->isAlive());
    rig.tweens.killTag("missing");
    rig.tweens.killAll();
    EXPECT_FALSE(newer->isAlive());
}

TEST(TweenManagerTest, StopsWhenTheTargetDies) {
    TweenRig rig;
    Box box;
    bool alive = true;
    auto tween = std::make_shared<core::PropertyTween>(1.0F);
    tween->addTrack(TweenRig::numberTrack(box, core::TweenProperty::to(10.0), [&alive] { return alive; }));
    rig.tweens.add(tween);
    rig.step(0.5);
    EXPECT_DOUBLE_EQ(box.x, 5.0);
    alive = false;
    rig.step(0.25);
    EXPECT_DOUBLE_EQ(box.x, 5.0);
    EXPECT_FALSE(tween->isAlive());
    EXPECT_THROW(tween->addTrack(TweenRig::numberTrack(box, core::TweenProperty::to(1.0))), std::logic_error);
}

TEST(TweenManagerTest, SpeedBasedTweensFindTheirDuration) {
    TweenRig rig;
    Box box;
    box.x = 10.0;
    auto tween = std::make_shared<core::PropertyTween>(20.0F, true);
    tween->addTrack(TweenRig::numberTrack(box, core::TweenProperty::to(50.0)));
    rig.tweens.add(tween);
    rig.step(1.0);
    EXPECT_FLOAT_EQ(tween->getDuration(), 2.0F);
    EXPECT_DOUBLE_EQ(box.x, 30.0);
}

TEST(TweenManagerTest, CallbacksMayChangeTweensWhileUpdating) {
    TweenRig rig;
    Box box;
    Box other;
    std::shared_ptr<core::PropertyTween> added;
    auto tween = rig.tweenX(box, 0.5F, core::TweenProperty::to(1.0));
    auto victim = rig.tweenX(other, 1.0F, core::TweenProperty::to(1.0));
    // clang-format off
    tween->getCallbacks().update = [&](float) {
        victim->kill();
        if (!added) {
            added = rig.tweenX(other, 1.0F, core::TweenProperty::to(-1.0));
        }
    };
    // clang-format on
    rig.step(0.25);
    EXPECT_FALSE(victim->isAlive());
    EXPECT_DOUBLE_EQ(other.x, 0.0);
    rig.step(0.5);
    EXPECT_DOUBLE_EQ(other.x, -0.5);

    auto self = rig.tweenX(box, 1.0F, core::TweenProperty::to(5.0));
    self->getCallbacks().start = [&] { self->kill(); };
    rig.step(0.5);
    EXPECT_FALSE(self->isAlive());

    // Clearing drops tweens without running their callbacks.
    int calls = 0;
    rig.tweenX(box, 1.0F, core::TweenProperty::to(5.0))->getCallbacks().kill = [&] { ++calls; };
    rig.tweens.clear();
    EXPECT_EQ(calls, 0);
    EXPECT_EQ(rig.tweens.size(), 0U);
}

TEST(TimelineTest, PlacesStepsJoinsInsertsAndLabels) {
    TweenRig rig;
    Box first;
    Box second;
    std::vector<std::string> log;
    auto timeline = std::make_shared<core::Timeline>();
    timeline->append(rig.tweenX(first, 1.0F, core::TweenProperty::to(10.0), false));
    timeline->join(rig.tweenX(second, 0.5F, core::TweenProperty::to(5.0), false));
    timeline->appendCall([&] { log.push_back("call " + std::to_string(static_cast<int>(first.x))); });
    timeline->addLabel("pause");
    timeline->appendInterval(0.5F);
    timeline->append(rig.tweenX(first, 1.0F, core::TweenProperty::to(0.0), false));
    timeline->insertCall(0.25F, [&] { log.emplace_back("early"); });
    timeline->insert("pause", rig.tweenX(second, 0.5F, core::TweenProperty::to(-5.0), false));
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
    auto timeline = std::make_shared<core::Timeline>();
    timeline->append(rig.tweenX(box, 1.0F, core::TweenProperty::to(10.0), false));
    timeline->appendCall([&] { calls.emplace_back("middle"); });
    timeline->append(rig.tweenX(box, 1.0F, core::TweenProperty::to(20.0), false));
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
    auto inner = std::make_shared<core::Timeline>();
    inner->appendCall([&] { ++calls; });
    inner->append(rig.tweenX(outer, 1.0F, core::TweenProperty::fromTo(0.0, 10.0), false));
    auto timeline = std::make_shared<core::Timeline>();
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

    std::vector<std::shared_ptr<core::Tween>> tweens;
    for (Box& box : boxes) {
        tweens.push_back(rig.tweenX(box, 1.0F, core::TweenProperty::to(1.0), false));
    }
    auto staggered = std::make_shared<core::Timeline>();
    staggered->stagger(tweens, 0.5F, core::Timeline::StaggerOrigin::Center);
    rig.tweens.add(staggered);
    rig.step(0.5);
    EXPECT_DOUBLE_EQ(boxes[1].x, 0.5);
    EXPECT_DOUBLE_EQ(boxes[0].x, 0.0);
    EXPECT_DOUBLE_EQ(boxes[2].x, 0.0);
    EXPECT_FLOAT_EQ(staggered->getDuration(), 1.5F);

    auto ends = std::make_shared<core::Timeline>();
    ends->stagger({rig.tweenX(boxes[0], 1.0F, core::TweenProperty::to(2.0), false), rig.tweenX(boxes[1], 1.0F, core::TweenProperty::to(2.0), false)}, 1.0F, core::Timeline::StaggerOrigin::End);
    EXPECT_FLOAT_EQ(ends->getDuration(), 2.0F);
    EXPECT_THROW(ends->stagger({}, 1.0F), std::invalid_argument);
}

TEST(TimelineTest, RejectsInvalidChildrenAndKillsTheirTweens) {
    TweenRig rig;
    Box box;
    auto timeline = std::make_shared<core::Timeline>();
    auto started = rig.tweenX(box, 1.0F, core::TweenProperty::to(1.0));
    rig.step(0.1);
    EXPECT_THROW(timeline->append(started), std::logic_error);
    EXPECT_THROW(timeline->append(timeline), std::logic_error);
    EXPECT_THROW(timeline->append(nullptr), std::invalid_argument);
    EXPECT_THROW(timeline->appendCall({}), std::invalid_argument);
    EXPECT_THROW(timeline->addLabel(""), std::invalid_argument);
    auto speed = std::make_shared<core::PropertyTween>(10.0F, true);
    EXPECT_THROW(timeline->append(speed), std::logic_error);

    // A tween playing in the manager moves into the timeline that takes it.
    auto moved = rig.tweenX(box, 1.0F, core::TweenProperty::to(2.0));
    auto killed = rig.tweenX(box, 1.0F, core::TweenProperty::to(3.0), false);
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

TEST(TweenManagerTest, OwnerScopesKillTweens) {
    TweenRig rig;
    Box box;
    auto tween = rig.tweenX(box, 1.0F, core::TweenProperty::to(1.0));
    {
        core::ConnectionScope scope;
        scope.add(tween->getConnection());
    }
    EXPECT_FALSE(tween->isAlive());
}

} // namespace haylen::core
