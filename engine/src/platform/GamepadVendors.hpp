#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string_view>
#include <utility>

#include "haylen/input/GamepadLabels.hpp"

namespace haylen::platform {

// Tells how a gamepad labels its face buttons from the USB vendor id of its maker, which Android, Linux and browsers report.
class GamepadVendors final {
  public:
    [[nodiscard]] static input::GamepadLabels getLabels(std::uint16_t vendor) noexcept;

    // Finds the vendor id in the id of a gamepad of the Gamepad API, which browsers write as `Vendor: 054c` inside the name or as `54c-9cc-` in front of it.
    [[nodiscard]] static std::optional<std::uint16_t> findInBrowserId(std::string_view id) noexcept;

  private:
    static constexpr std::array<std::pair<std::uint16_t, input::GamepadLabels>, 3> kVendors{{
        {0x045E, input::GamepadLabels::Letters},
        {0x054C, input::GamepadLabels::Shapes},
        {0x057E, input::GamepadLabels::SwappedLetters},
    }};

    [[nodiscard]] static std::optional<std::uint16_t> parseHex(std::string_view digits) noexcept;
};

} // namespace haylen::platform
