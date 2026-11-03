#pragma once

#include <array>
#include <bitset>
#include <string>

#include "haylen/input/Controls.hpp"
#include "haylen/input/GamepadLabels.hpp"
#include "haylen/input/MotionState.hpp"
#include "haylen/input/TouchpadFinger.hpp"

namespace haylen::input {

// The raw state of one gamepad as the platform reports it, before the dead zone applies, with the motion sensors it has, the readings of those that run, which run only while the app asks for them, and the fingers on its touchpads.
struct GamepadState {
    bool connected = false;
    std::string name;
    GamepadLabels labels = GamepadLabels::Unknown;
    std::array<bool, Controls::kGamepadButtonCount> buttons{};
    std::array<float, Controls::kGamepadAxisCount> axes{};
    std::bitset<Controls::kMotionSensorCount> motionSensors;
    MotionState motion;
    std::array<TouchpadFinger, Controls::kTouchpadFingerCount> touchpad{};
};

} // namespace haylen::input
