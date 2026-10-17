#include <gtest/gtest.h>

#include <algorithm>
#include <string>

#include "haylen/core/Engine.hpp"
#include "haylen/math/Rect.hpp"
#include "haylen/ui/Gui.hpp"
#include "support/UiFixture.hpp"

namespace haylen::ui {

namespace {

class ScrollArbiterTest : public ::testing::Test, public test::UiFixture {
  protected:
    // A frame time long enough for a gesture of the wheel to end and for every motion to settle.
    static constexpr int kRest = 60;

    [[nodiscard]] static std::string labels(int count) {
        std::string list;
        for (int index = 1; index <= count; ++index) {
            list += (index > 1 ? ", " : "") + std::string(R"({"kind": "label", "text": "Line )") + std::to_string(index) + R"("})";
        }
        return list;
    }

    // A vertical page with a horizontal strip wider than the page, and content around it that makes the page scroll.
    std::shared_ptr<Gui> mountPage() {
        // clang-format off
        return mount(R"({"kind": "scroll", "id": "page", "width": 600, "height": 400, "children": [{"kind": "column", "gap": 0, "children": [
            {"kind": "spacer", "height": 200},
            {"kind": "scroll", "id": "strip", "axis": "horizontal", "scrollbar": false, "width": 500, "height": 150, "children": [{"kind": "row", "id": "covers", "children": [{"kind": "spacer", "width": 1500, "height": 100}]}]},
            {"kind": "spacer", "height": 1500}
        ]}]})");
        // clang-format on
    }

    // Sends a trackpad scroll over a point and runs a frame.
    void trackpad(math::Vec2 point, math::Vec2 scroll) {
        wheel(point, scroll.y, scroll.x, true);
    }

    // Sends the scroll of a trackpad that reports the phases of its gestures over a point, up and down, and runs a frame.
    void phase(math::Vec2 point, float scroll, platform::Event::ScrollPhase value) {
        pointer(platform::Event::Type::MouseMove, point);
        platform::Event event;
        event.type = platform::Event::Type::MouseScroll;
        event.scroll = {0.0F, scroll};
        event.preciseScroll = true;
        event.scrollPhase = value;
        getEngine().handleEvent(event);
        frames();
    }

    std::shared_ptr<Gui> mountLongPage() {
        return mount(R"({"kind": "scroll", "id": "page", "width": 600, "height": 400, "children": [{"kind": "column", "id": "content", "children": [{"kind": "spacer", "height": 4000}]}]})");
    }

    // Drags a finger from one point to another over some frames, rests it until its speed is gone and lifts it.
    void swipe(math::Vec2 from, math::Vec2 to, int steps = 10) {
        touch(platform::Event::Type::TouchBegan, 1, from);
        frames();
        for (int step = 1; step <= steps; ++step) {
            touch(platform::Event::Type::TouchMoved, 1, from + (to - from) * (static_cast<float>(step) / static_cast<float>(steps)));
            frames();
        }
        frames(20);
        touch(platform::Event::Type::TouchEnded, 1, to);
        frames(kRest);
    }
};

} // namespace

TEST_F(ScrollArbiterTest, AVerticalGestureOverAHorizontalAreaScrollsTheVerticalAreaAroundIt) {
    auto gui = mountPage();
    const float top = getBounds(*gui, "strip").y;
    const float left = getBounds(*gui, "covers").x;

    // A trackpad that moves up and down over the strip scrolls the page, and the strip keeps its place along its axis.
    trackpad(getBounds(*gui, "strip").getCenter(), {0.0F, -40.0F});
    frames(kRest);
    EXPECT_NEAR(getBounds(*gui, "strip").y, top - 40.0F, 0.5F);
    EXPECT_EQ(getBounds(*gui, "covers").x, left);

    // A sideways gesture over the strip scrolls only the strip.
    const float scrolledTop = getBounds(*gui, "strip").y;
    trackpad(getBounds(*gui, "strip").getCenter(), {-40.0F, 0.0F});
    frames(kRest);
    EXPECT_NEAR(getBounds(*gui, "covers").x, left - 40.0F, 0.5F);
    EXPECT_EQ(getBounds(*gui, "strip").y, scrolledTop);

    // The wheel of a mouse turns up and down, so it scrolls the page too.
    wheel(getBounds(*gui, "strip").getCenter(), -1.0F);
    frames(kRest);
    EXPECT_NEAR(getBounds(*gui, "strip").y, scrolledTop - 64.0F, 0.5F);
    EXPECT_NEAR(getBounds(*gui, "covers").x, left - 40.0F, 0.5F);
}

TEST_F(ScrollArbiterTest, AGestureKeepsTheAxisItStartsAlong) {
    auto gui = mountPage();
    const math::Vec2 inside = getBounds(*gui, "strip").getCenter();
    const float top = getBounds(*gui, "strip").y;
    const float left = getBounds(*gui, "covers").x;

    // A gesture that starts mostly up and down stays with the page, also when it drifts sideways later.
    trackpad(inside, {-6.0F, -30.0F});
    trackpad(inside, {-40.0F, -2.0F});
    trackpad(inside, {-40.0F, 0.0F});
    frames(kRest);
    EXPECT_NEAR(getBounds(*gui, "strip").y, top - 32.0F, 0.5F);
    EXPECT_EQ(getBounds(*gui, "covers").x, left);
}

TEST_F(ScrollArbiterTest, AGestureStaysWithTheAreaItStartedInWhileTheContentMovesUnderThePointer) {
    // clang-format off
    auto gui = mount(R"({"kind": "scroll", "id": "page", "width": 600, "height": 400, "children": [{"kind": "column", "id": "pageContent", "gap": 0, "children": [
        {"kind": "spacer", "height": 300},
        {"kind": "scroll", "id": "inner", "width": 500, "height": 200, "children": [{"kind": "column", "id": "innerContent", "children": [)" + labels(40) + R"(]}]},
        {"kind": "spacer", "height": 1500}
    ]}]})");
    // clang-format on
    const math::Vec2 point = getBounds(*gui, "page").getMin() + math::Vec2{250.0F, 250.0F};
    const float page = getBounds(*gui, "pageContent").y;
    const float inner = getBounds(*gui, "innerContent").y - getBounds(*gui, "inner").y;

    // The gesture starts over the page, and the inner area that its scroll brings under the pointer never takes it.
    for (int step = 0; step < 5; ++step) {
        trackpad(point, {0.0F, -40.0F});
    }
    frames(kRest);
    EXPECT_TRUE(getBounds(*gui, "inner").contains(point));
    EXPECT_NEAR(getBounds(*gui, "pageContent").y, page - 200.0F, 0.5F);
    EXPECT_EQ(getBounds(*gui, "innerContent").y - getBounds(*gui, "inner").y, inner);

    // The next gesture starts over the inner area, which takes it.
    trackpad(point, {0.0F, -40.0F});
    frames(kRest);
    EXPECT_NEAR(getBounds(*gui, "pageContent").y, page - 200.0F, 0.5F);
    EXPECT_NEAR(getBounds(*gui, "innerContent").y - getBounds(*gui, "inner").y, inner - 40.0F, 0.5F);
}

TEST_F(ScrollArbiterTest, AnAreaAtItsEndLeavesANewGestureToTheAreaAroundIt) {
    // clang-format off
    auto gui = mount(R"({"kind": "scroll", "id": "page", "width": 600, "height": 400, "children": [{"kind": "column", "id": "pageContent", "gap": 0, "children": [
        {"kind": "scroll", "id": "inner", "width": 500, "height": 300, "children": [{"kind": "column", "id": "innerContent", "children": [)" + labels(40) + R"(]}]},
        {"kind": "spacer", "height": 1500}
    ]}]})");
    // clang-format on
    const float page = getBounds(*gui, "pageContent").y;
    trackpad(getBounds(*gui, "page").getMin() + math::Vec2{250.0F, 350.0F}, {0.0F, -50.0F});
    frames(kRest);
    ASSERT_NEAR(getBounds(*gui, "pageContent").y, page - 50.0F, 0.5F);

    // The inner area shows its start, so a gesture up over it scrolls the page back instead.
    const float inner = getBounds(*gui, "innerContent").y - getBounds(*gui, "inner").y;
    trackpad(getBounds(*gui, "inner").getCenter(), {0.0F, 20.0F});
    frames(kRest);
    EXPECT_NEAR(getBounds(*gui, "pageContent").y, page - 30.0F, 0.5F);
    EXPECT_EQ(getBounds(*gui, "innerContent").y - getBounds(*gui, "inner").y, inner);
}

TEST_F(ScrollArbiterTest, ShiftTurnsAWheelSideways) {
    auto gui = mountPage();
    const float top = getBounds(*gui, "strip").y;
    const float left = getBounds(*gui, "covers").x;
    pointer(platform::Event::Type::MouseMove, getBounds(*gui, "strip").getCenter());
    platform::Event event;
    event.type = platform::Event::Type::MouseScroll;
    event.scroll = {0.0F, -1.0F};
    event.modifiers.shift = true;
    getEngine().handleEvent(event);
    frames(kRest);
    EXPECT_NEAR(getBounds(*gui, "covers").x, left - 64.0F, 0.5F);
    EXPECT_EQ(getBounds(*gui, "strip").y, top);
}

// Content that fingers on a trackpad pulled past an end springs back the moment they leave, while the momentum the system adds after them still arrives and moves nothing.
TEST_F(ScrollArbiterTest, SpringsBackTheMomentFingersLeaveATrackpadWhileItsMomentumGoesOn) {
    using Phase = platform::Event::ScrollPhase;
    auto gui = mountLongPage();
    const math::Vec2 inside = getBounds(*gui, "page").getCenter();
    const float top = getBounds(*gui, "content").y;
    phase(inside, 0.0F, Phase::Began);
    for (int step = 0; step < 6; ++step) {
        phase(inside, 40.0F, Phase::Changed);
    }
    const float stretched = getBounds(*gui, "content").y - top;
    EXPECT_GT(stretched, 40.0F);
    EXPECT_LT(stretched, 240.0F);

    // The content goes on a little with the speed it had and then springs back, never pulled further by the momentum.
    phase(inside, 0.0F, Phase::Ended);
    float peak = stretched;
    for (int step = 0; step < 30; ++step) {
        phase(inside, 20.0F, Phase::Momentum);
        const float now = getBounds(*gui, "content").y - top;
        peak = std::max(peak, now);
        if (step == 0) {
            EXPECT_GT(now, stretched * 0.5F);
        }
    }
    EXPECT_LT(peak, stretched + 20.0F);
    EXPECT_NEAR(getBounds(*gui, "content").y, top, 0.5F);
}

// Fingers that leave a trackpad in the middle of the content fling it with the speed they had, the momentum of the system changes nothing, and fingers that touch the trackpad again stop it.
TEST_F(ScrollArbiterTest, FlingsWithItsOwnMotionWhenFingersLeaveATrackpad) {
    using Phase = platform::Event::ScrollPhase;
    auto gui = mountLongPage();
    const math::Vec2 inside = getBounds(*gui, "page").getCenter();
    trackpad(inside, {0.0F, -1000.0F});
    frames(kRest);
    phase(inside, 0.0F, Phase::Began);
    for (int step = 0; step < 6; ++step) {
        phase(inside, -40.0F, Phase::Changed);
    }
    phase(inside, 0.0F, Phase::Ended);
    const float released = getBounds(*gui, "content").y;
    for (int step = 0; step < 10; ++step) {
        phase(inside, 300.0F, Phase::Momentum);
    }
    const float flung = getBounds(*gui, "content").y;
    EXPECT_LT(flung, released - 100.0F);

    phase(inside, 0.0F, Phase::Began);
    const float caught = getBounds(*gui, "content").y;
    frames(10);
    EXPECT_NEAR(getBounds(*gui, "content").y, caught, 1.0F);
    phase(inside, 0.0F, Phase::Ended);
}

// A precise device without phases never pulls the content past an end, since nothing would tell it to spring back.
TEST_F(ScrollArbiterTest, NeverStretchesTheContentWithAPreciseDeviceWithoutPhases) {
    auto gui = mountLongPage();
    const math::Vec2 inside = getBounds(*gui, "page").getCenter();
    const float top = getBounds(*gui, "content").y;
    for (int step = 0; step < 6; ++step) {
        trackpad(inside, {0.0F, 40.0F});
        EXPECT_EQ(getBounds(*gui, "content").y, top);
    }
}

TEST_F(ScrollArbiterTest, AFingerGoesToTheAreaWhoseAxisItsDragFollows) {
    auto gui = mountPage();
    const math::Vec2 inside = getBounds(*gui, "strip").getCenter();
    const float top = getBounds(*gui, "strip").y;
    const float left = getBounds(*gui, "covers").x;

    // A finger that drags up over the strip scrolls the page.
    swipe(inside, inside - math::Vec2{6.0F, 200.0F});
    EXPECT_LT(getBounds(*gui, "strip").y, top - 150.0F);
    EXPECT_EQ(getBounds(*gui, "covers").x, left);

    // A finger that drags sideways scrolls the strip, and the page stays still while the finger wavers up and down.
    const math::Vec2 moved = getBounds(*gui, "strip").getCenter();
    const float scrolledTop = getBounds(*gui, "strip").y;
    swipe(moved, moved - math::Vec2{200.0F, 30.0F});
    EXPECT_LT(getBounds(*gui, "covers").x, left - 150.0F);
    EXPECT_EQ(getBounds(*gui, "strip").y, scrolledTop);
}

TEST_F(ScrollArbiterTest, AFingerGoesToTheInnermostAreaAlongTheSameAxis) {
    // clang-format off
    auto gui = mount(R"({"kind": "scroll", "id": "page", "width": 600, "height": 400, "children": [{"kind": "column", "id": "pageContent", "gap": 0, "children": [
        {"kind": "scroll", "id": "inner", "width": 500, "height": 300, "children": [{"kind": "column", "id": "innerContent", "children": [)" + labels(40) + R"(]}]},
        {"kind": "spacer", "height": 1500}
    ]}]})");
    // clang-format on
    const float page = getBounds(*gui, "pageContent").y;
    const float inner = getBounds(*gui, "innerContent").y;
    const math::Vec2 inside = getBounds(*gui, "inner").getCenter();
    swipe(inside + math::Vec2{0.0F, 100.0F}, inside - math::Vec2{0.0F, 100.0F});
    EXPECT_EQ(getBounds(*gui, "pageContent").y, page);
    EXPECT_LT(getBounds(*gui, "innerContent").y, inner - 150.0F);
}

} // namespace haylen::ui
