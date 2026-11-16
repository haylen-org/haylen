#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <optional>
#include <vector>

#include "haylen/input/GamepadAxis.hpp"
#include "haylen/input/Input.hpp"
#include "platform/worker/Gamepads.hpp"

namespace haylen::platform::worker {

class WorkerGamepadsTest : public ::testing::Test {
  protected:
    [[nodiscard]] std::array<input::GamepadState, input::Input::kMaxGamepads> poll() const {
        std::array<input::GamepadState, input::Input::kMaxGamepads> states{};
        gamepads.poll(states);
        return states;
    }

    Gamepads gamepads;
};

TEST_F(WorkerGamepadsTest, ReadsTheStandardMapping) {
    Gamepads::Record record{.index = 0, .id = "Wireless Controller (STANDARD GAMEPAD Vendor: 054c Product: 09cc)"};
    record.buttons[0] = 1.0F;
    record.buttons[12] = 1.0F;
    record.buttons[16] = 1.0F;
    record.buttons[7] = 0.25F;
    record.axes = {-0.5F, 1.0F, 0.0F, 0.75F};
    gamepads.update(std::vector{record});

    const auto states = poll();
    const input::GamepadState& state = states[0];
    EXPECT_TRUE(state.connected);
    EXPECT_EQ(state.labels, input::GamepadLabels::Shapes);
    EXPECT_TRUE(state.buttons[static_cast<std::size_t>(input::GamepadButton::South)]);
    EXPECT_TRUE(state.buttons[static_cast<std::size_t>(input::GamepadButton::DpadUp)]);
    EXPECT_TRUE(state.buttons[static_cast<std::size_t>(input::GamepadButton::Guide)]);
    EXPECT_FALSE(state.buttons[static_cast<std::size_t>(input::GamepadButton::East)]);
    EXPECT_FLOAT_EQ(state.axes[static_cast<std::size_t>(input::GamepadAxis::LeftX)], -0.5F);
    EXPECT_FLOAT_EQ(state.axes[static_cast<std::size_t>(input::GamepadAxis::RightY)], 0.75F);
    EXPECT_FLOAT_EQ(state.axes[static_cast<std::size_t>(input::GamepadAxis::RightTrigger)], 0.25F);
    EXPECT_FALSE(states[1].connected);
}

TEST_F(WorkerGamepadsTest, KeepsTheSlotOfAControllerThatStaysConnected) {
    const Gamepads::Record first{.index = 0, .id = "054c-09cc-Wireless Controller"};
    const Gamepads::Record second{.index = 1, .id = "Xbox 360 Controller (XInput STANDARD GAMEPAD)"};
    gamepads.update(std::vector{first, second});
    EXPECT_EQ(gamepads.getIndex(0), std::optional<std::size_t>(0));
    EXPECT_EQ(gamepads.getIndex(1), std::optional<std::size_t>(1));

    gamepads.update(std::vector{second});
    EXPECT_FALSE(gamepads.getIndex(0));
    EXPECT_EQ(gamepads.getIndex(1), std::optional<std::size_t>(1));

    const auto states = poll();
    EXPECT_FALSE(states[0].connected);
    EXPECT_EQ(states[1].labels, input::GamepadLabels::Letters);
}

TEST_F(WorkerGamepadsTest, FindsTheVendorWhereTheBrowserWritesIt) {
    EXPECT_EQ(Gamepads::findVendor("Pro Controller (STANDARD GAMEPAD Vendor: 057e Product: 2009)"), std::optional<std::uint16_t>(0x057E));
    EXPECT_EQ(Gamepads::findVendor("054c-0ce6-DualSense Wireless Controller"), std::optional<std::uint16_t>(0x054C));
    EXPECT_EQ(Gamepads::findVendor("Xbox 360 Controller (XInput STANDARD GAMEPAD)"), std::optional<std::uint16_t>(0x045E));
    EXPECT_FALSE(Gamepads::findVendor("Generic USB Joystick"));
    EXPECT_FALSE(Gamepads::findVendor("zzzz-0000-Broken"));
}

} // namespace haylen::platform::worker
