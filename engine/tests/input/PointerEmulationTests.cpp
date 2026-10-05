#include <gtest/gtest.h>

#include <span>
#include <vector>

#include "haylen/input/PointerEmulation.hpp"
#include "haylen/platform/Event.hpp"

namespace haylen::input {

namespace {

class PointerEmulationTest : public ::testing::Test {
  protected:
    [[nodiscard]] static platform::Event makeMouse(platform::Event::Type type, MouseButton button = MouseButton::Left) {
        return {.type = type, .mouseButton = button, .position = {40.0F, 50.0F}};
    }

    [[nodiscard]] static platform::Event makeTouch(platform::Event::Type type, std::uint64_t id, math::Vec2 position) {
        platform::Event event{.type = type, .touchCount = 1};
        event.touches[0] = {.id = id, .position = position, .changed = true};
        return event;
    }

    [[nodiscard]] static std::vector<platform::Event::Type> getTypes(std::span<const platform::Event> events) {
        std::vector<platform::Event::Type> types;
        for (const platform::Event& event : events) {
            types.push_back(event.type);
        }
        return types;
    }

    PointerEmulation emulation;
};

using Type = platform::Event::Type;

} // namespace

TEST_F(PointerEmulationTest, LeavesEventsAloneUntilAsked) {
    const platform::Event press = makeMouse(Type::MouseDown);
    const std::span<const platform::Event> events = emulation.convert(press);
    ASSERT_EQ(events.size(), 1U);
    EXPECT_EQ(&events[0], &press);
    EXPECT_EQ(emulation.convert(makeTouch(Type::TouchBegan, 3, {})).size(), 1U);
}

TEST_F(PointerEmulationTest, TurnsTheLeftMouseButtonIntoAFinger) {
    emulation.setMouseAsTouch(true);
    EXPECT_TRUE(emulation.convert(makeMouse(Type::MouseMove)).empty());

    const std::span<const platform::Event> began = emulation.convert(makeMouse(Type::MouseDown));
    ASSERT_EQ(getTypes(began), (std::vector{Type::TouchBegan}));
    EXPECT_EQ(began[0].touchCount, 1U);
    EXPECT_EQ(began[0].touches[0].id, PointerEmulation::kMouseTouchId);
    EXPECT_EQ(began[0].touches[0].position, math::Vec2(40.0F, 50.0F));
    EXPECT_EQ(getTypes(emulation.convert(makeMouse(Type::MouseMove))), (std::vector{Type::TouchMoved}));
    EXPECT_EQ(getTypes(emulation.convert(makeMouse(Type::MouseUp))), (std::vector{Type::TouchEnded}));
    EXPECT_TRUE(emulation.convert(makeMouse(Type::MouseMove)).empty());

    // The other buttons and the wheel stay mouse input.
    EXPECT_EQ(getTypes(emulation.convert(makeMouse(Type::MouseDown, MouseButton::Right))), (std::vector{Type::MouseDown}));
    EXPECT_EQ(getTypes(emulation.convert(makeMouse(Type::MouseScroll))), (std::vector{Type::MouseScroll}));
}

TEST_F(PointerEmulationTest, LetsTheFirstFingerDriveTheMouse) {
    emulation.setTouchAsMouse(true);
    EXPECT_EQ(getTypes(emulation.convert(makeTouch(Type::TouchBegan, 1, {10.0F, 20.0F}))), (std::vector{Type::MouseMove, Type::MouseDown, Type::TouchBegan}));

    // A second finger is a touch only, and the first one keeps the mouse until it lifts.
    EXPECT_EQ(getTypes(emulation.convert(makeTouch(Type::TouchBegan, 2, {300.0F, 20.0F}))), (std::vector{Type::TouchBegan}));
    EXPECT_EQ(getTypes(emulation.convert(makeTouch(Type::TouchMoved, 2, {310.0F, 20.0F}))), (std::vector{Type::TouchMoved}));
    const std::span<const platform::Event> moved = emulation.convert(makeTouch(Type::TouchMoved, 1, {15.0F, 25.0F}));
    ASSERT_EQ(getTypes(moved), (std::vector{Type::MouseMove, Type::TouchMoved}));
    EXPECT_EQ(moved[0].position, math::Vec2(15.0F, 25.0F));
    EXPECT_EQ(getTypes(emulation.convert(makeTouch(Type::TouchCancelled, 1, {15.0F, 25.0F}))), (std::vector{Type::MouseMove, Type::MouseUp, Type::TouchCancelled}));

    // The next finger down takes the mouse over.
    EXPECT_EQ(getTypes(emulation.convert(makeTouch(Type::TouchEnded, 2, {310.0F, 20.0F}))), (std::vector{Type::TouchEnded}));
    EXPECT_EQ(getTypes(emulation.convert(makeTouch(Type::TouchBegan, 4, {50.0F, 60.0F}))), (std::vector{Type::MouseMove, Type::MouseDown, Type::TouchBegan}));
}

} // namespace haylen::input
