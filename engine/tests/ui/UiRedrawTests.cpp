#include <gtest/gtest.h>

#include <cstdint>
#include <memory>
#include <string>

#include "gpu/NullDevice.hpp"
#include "haylen/core/Engine.hpp"
#include "haylen/input/Key.hpp"
#include "haylen/ui/Gui.hpp"
#include "haylen/ui/Navigation.hpp"
#include "platform/headless/HeadlessHost.hpp"
#include "support/AllocationTracker.hpp"
#include "support/UiFixture.hpp"

namespace haylen::ui {

// An app that redraws on change draws the frames in which its UI moves or waits for time, and none once it rests.
class UiRedrawTest : public ::testing::Test, public test::UiFixture {
  protected:
    static constexpr std::uint64_t kSettle = core::Engine::kSettleFrames;

    UiRedrawTest() : UiFixture({{"haylen.json", R"({"name": "Test App", "identifier": "com.haylen.tests", "window": {"redraw": "onChange"}})"}}) {
        frames(10);
    }

    // Runs frames and returns how many of them the device committed.
    [[nodiscard]] std::uint64_t drawn(int count) {
        const gpu::NullDevice& device = getFixture().host().getGpu();
        const std::uint64_t before = device.getCommittedFrames();
        frames(count);
        return device.getCommittedFrames() - before;
    }
};

TEST_F(UiRedrawTest, GuiAtRestDrawsAndAllocatesNothing) {
    auto gui = mount(R"({"kind": "column", "children": [{"kind": "label", "text": "Rest"}, {"kind": "button", "id": "press", "text": "Press"}, {"kind": "toggle", "id": "sound", "checked": true}]})");
    frames(10);

    const test::AllocationTracker tracker;
    EXPECT_EQ(drawn(60), 0U);
    EXPECT_EQ(tracker.getTotal(), 0U);
}

// A GUI that comes or goes changes the screen, whatever code mounted it, and a new GUI draws until it laid out complete.
TEST_F(UiRedrawTest, GuiThatMountsOrUnmountsDraws) {
    auto gui = mount(R"({"kind": "label", "text": "Arrived"})");
    EXPECT_GE(drawn(10), kSettle);
    EXPECT_EQ(drawn(10), 0U);

    ASSERT_TRUE(getUi().unmount(*gui));
    EXPECT_EQ(drawn(10), 1U + kSettle);
}

// The content glides after the wheel, so the frames keep drawing until it rests at its place.
TEST_F(UiRedrawTest, ScrollThatGlidesDrawsUntilItRests) {
    auto gui = mount(R"({"kind": "scroll", "id": "area", "width": 300, "height": 240, "children": [{"kind": "column", "children": [)" + labels(80) + "]}]}");
    frames(10);
    ASSERT_EQ(drawn(10), 0U);

    wheel(getBounds(*gui, "area").getCenter(), -3.0F);
    EXPECT_GT(drawn(120), 1U + kSettle);
    EXPECT_EQ(drawn(60), 0U);
}

TEST_F(UiRedrawTest, ToggleDrawsWhileItsKnobSlides) {
    auto gui = mount(R"({"kind": "column", "padding": 20, "children": [{"kind": "toggle", "id": "sound", "checked": true}]})");
    frames(10);
    click(*gui, "sound");
    EXPECT_GT(drawn(getTransitionFrames()), 0U);
    frames(getTransitionFrames());
    EXPECT_EQ(drawn(30), 0U);
}

// A message counts its time only in the frames that draw, so every frame draws while it shows and the screen rests once it left.
TEST_F(UiRedrawTest, MessageDrawsWhileItShows) {
    static_cast<void>(getUi().getNavigation().showMessage({{"text", "Saved"}, {"duration", 0.5}}));
    EXPECT_EQ(drawn(30), 30U);
    frames(120);
    EXPECT_EQ(drawn(30), 0U);
}

// The caret blinks with the time of the frames, so a field draws every frame while it edits and the screen rests once it stops.
TEST_F(UiRedrawTest, FieldThatEditsDrawsItsCaret) {
    auto gui = mount(R"({"kind": "column", "padding": 20, "children": [{"kind": "textField", "id": "name", "placeholder": "Name"}]})");
    frames(10);
    click(*gui, "name");
    EXPECT_EQ(drawn(30), 30U);

    key(input::Key::Escape);
    frames(10);
    EXPECT_EQ(drawn(30), 0U);
}

} // namespace haylen::ui
