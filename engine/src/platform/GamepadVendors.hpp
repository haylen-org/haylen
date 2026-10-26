#pragma once

#include <array>
#include <cstdint>
#include <utility>

#include "haylen/input/GamepadLabels.hpp"

namespace haylen::platform {

// Tells how a gamepad labels its face buttons from the USB vendor id of its maker, which Android reports.
class GamepadVendors final {
  public:
    [[nodiscard]] static input::GamepadLabels getLabels(std::uint16_t vendor) noexcept;

  private:
    static constexpr std::array<std::pair<std::uint16_t, input::GamepadLabels>, 3> kVendors{{
        {0x045E, input::GamepadLabels::Letters},
        {0x054C, input::GamepadLabels::Shapes},
        {0x057E, input::GamepadLabels::SwappedLetters},
    }};
};

} // namespace haylen::platform
