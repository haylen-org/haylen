#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string_view>
#include <utility>

#include "haylen/core/Json.hpp"

namespace haylen::platform {

// The battery of the device as the platform last reported it. The state is `None` on devices without a battery and `Unknown` where the platform does not tell it.
struct Battery {
    enum class State : std::uint8_t {
        Unknown,
        Charging,
        Discharging,
        Full,
        None,
    };

    // The charge from 0 to 1, empty where the platform does not tell it.
    std::optional<float> level;
    bool charging = false;
    State state = State::Unknown;

    [[nodiscard]] bool operator==(const Battery&) const = default;

    // The battery as the `batteryChanged` event and `haylen.system` report it, with `level` left out while it is empty.
    [[nodiscard]] core::Json toJson() const;

    [[nodiscard]] static std::string_view stateName(State value) noexcept;

  private:
    static const std::array<std::pair<std::string_view, State>, 5> kStateNames;
};

} // namespace haylen::platform
