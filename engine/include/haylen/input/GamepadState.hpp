#pragma once

#include <array>
#include <string>

#include "haylen/input/Controls.hpp"
#include "haylen/input/GamepadLabels.hpp"

namespace haylen::input {

// The raw state of one gamepad as the platform reports it, before the dead zone applies.
struct GamepadState {
    bool connected = false;
    std::string name;
    GamepadLabels labels = GamepadLabels::Unknown;
    std::array<bool, Controls::kGamepadButtonCount> buttons{};
    std::array<float, Controls::kGamepadAxisCount> axes{};
};

} // namespace haylen::input
