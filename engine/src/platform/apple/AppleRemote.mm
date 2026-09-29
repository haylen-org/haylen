#import "platform/apple/AppleRemote.hpp"

namespace haylen::platform {

void AppleRemote::read(GCMicroGamepad* remote, input::GamepadState& state) {
    // The touch surface reports how far the finger moved since it landed, with up as positive, while the engine follows the screen with down as positive.
    remote.reportsAbsoluteDpadValues = NO;
    const float x = remote.dpad.xAxis.value;
    const float y = -remote.dpad.yAxis.value;
    state.axes[static_cast<std::size_t>(input::GamepadAxis::LeftX)] = x;
    state.axes[static_cast<std::size_t>(input::GamepadAxis::LeftY)] = y;
    state.buttons[static_cast<std::size_t>(input::GamepadButton::DpadLeft)] = x < -kPressThreshold;
    state.buttons[static_cast<std::size_t>(input::GamepadButton::DpadRight)] = x > kPressThreshold;
    state.buttons[static_cast<std::size_t>(input::GamepadButton::DpadUp)] = y < -kPressThreshold;
    state.buttons[static_cast<std::size_t>(input::GamepadButton::DpadDown)] = y > kPressThreshold;
    state.buttons[static_cast<std::size_t>(input::GamepadButton::South)] = remote.buttonA.pressed;
    state.buttons[static_cast<std::size_t>(input::GamepadButton::West)] = remote.buttonX.pressed;
}

} // namespace haylen::platform
