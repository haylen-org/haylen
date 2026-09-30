#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <initializer_list>

#include "haylen/input/Controls.hpp"
#include "haylen/input/GamepadAxis.hpp"
#include "haylen/input/GamepadButton.hpp"
#include "haylen/input/GamepadState.hpp"
#include "haylen/input/Input.hpp"
#include "platform/android/AndroidGamepadStates.hpp"

namespace haylen::platform {

class AndroidGamepadStatesTest : public ::testing::Test {
  protected:
    using States = std::array<input::GamepadState, input::Input::kMaxGamepads>;

    static States read(const AndroidGamepadStates& controllers) {
        States states{};
        controllers.copyTo(states);
        return states;
    }

    static float axis(const input::GamepadState& state, input::GamepadAxis value) {
        return state.axes[static_cast<std::size_t>(value)];
    }

    static bool button(const input::GamepadState& state, input::GamepadButton value) {
        return state.buttons[static_cast<std::size_t>(value)];
    }
};

TEST_F(AndroidGamepadStatesTest, ReleasesTheAxesOfEveryControllerWhenTheWindowLosesTheFocus) {
    AndroidGamepadStates controllers;
    controllers.move(7, {.leftX = 0.8F, .rightY = -0.5F, .leftTrigger = 1.0F, .rightTrigger = 0.4F, .hatX = -1.0F});
    controllers.move(9, {.leftY = 0.6F, .hatY = 1.0F});
    controllers.press(7, input::GamepadButton::South, true);
    States states = read(controllers);
    EXPECT_FLOAT_EQ(axis(states[0], input::GamepadAxis::LeftX), 0.8F);
    EXPECT_FLOAT_EQ(axis(states[0], input::GamepadAxis::LeftTrigger), 1.0F);
    EXPECT_TRUE(button(states[0], input::GamepadButton::DpadLeft));
    EXPECT_FLOAT_EQ(axis(states[1], input::GamepadAxis::LeftY), 0.6F);
    EXPECT_TRUE(button(states[1], input::GamepadButton::DpadDown));

    // The sticks, the triggers and the directional pad of the hat come to rest, while the controllers keep their slots and the buttons wait for their releases, which Android still delivers.
    controllers.releaseAxes();
    states = read(controllers);
    for (const std::size_t slot : {0U, 1U}) {
        EXPECT_TRUE(states[slot].connected);
        EXPECT_EQ(states[slot].axes, (std::array<float, input::Controls::kGamepadAxisCount>{}));
        EXPECT_FALSE(button(states[slot], input::GamepadButton::DpadLeft));
        EXPECT_FALSE(button(states[slot], input::GamepadButton::DpadDown));
    }
    EXPECT_TRUE(button(states[0], input::GamepadButton::South));
    EXPECT_FALSE(states[2].connected);

    // The next motion event brings the position of the stick back.
    controllers.move(9, {.leftY = -0.3F});
    EXPECT_FLOAT_EQ(axis(read(controllers)[1], input::GamepadAxis::LeftY), -0.3F);
}

TEST_F(AndroidGamepadStatesTest, KeepsEveryControllerInTheSlotOfItsFirstEvent) {
    AndroidGamepadStates controllers;
    controllers.press(4, input::GamepadButton::South, true);
    controllers.move(-1, {.leftX = 0.5F, .hatX = 1.0F, .hatY = -1.0F});
    controllers.press(8, input::GamepadButton::East, true);
    controllers.press(12, input::GamepadButton::North, true);
    controllers.press(20, input::GamepadButton::West, true);
    States states = read(controllers);
    EXPECT_TRUE(button(states[0], input::GamepadButton::South));
    EXPECT_FLOAT_EQ(axis(states[1], input::GamepadAxis::LeftX), 0.5F);
    EXPECT_TRUE(button(states[1], input::GamepadButton::DpadRight));
    EXPECT_TRUE(button(states[1], input::GamepadButton::DpadUp));
    EXPECT_TRUE(button(states[2], input::GamepadButton::East));
    EXPECT_TRUE(button(states[3], input::GamepadButton::North));
    for (const input::GamepadState& state : states) {
        EXPECT_TRUE(state.connected);
        EXPECT_EQ(state.name, "Controller");
        EXPECT_FALSE(button(state, input::GamepadButton::West)) << "A fifth controller waits for a free slot.";
    }

    // A removed controller frees its slot for the next one, and the others stay where they are.
    controllers.remove(-1);
    EXPECT_FALSE(read(controllers)[1].connected);
    controllers.press(20, input::GamepadButton::West, true);
    controllers.press(4, input::GamepadButton::South, false);
    states = read(controllers);
    EXPECT_TRUE(states[1].connected);
    EXPECT_TRUE(button(states[1], input::GamepadButton::West));
    EXPECT_FLOAT_EQ(axis(states[1], input::GamepadAxis::LeftX), 0.0F);
    EXPECT_FALSE(button(states[0], input::GamepadButton::South));
    EXPECT_TRUE(button(states[2], input::GamepadButton::East));
}

} // namespace haylen::platform
