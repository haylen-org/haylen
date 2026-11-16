#pragma once

#include "haylen/input/KeyModifiers.hpp"

namespace haylen::platform {

// The modifier keys that the script of the page hands the runtime with its input, as the bits of shift, control, alt and the super key, from the lowest.
class PageModifiers final {
  public:
    [[nodiscard]] static constexpr input::KeyModifiers read(int bits) noexcept {
        return {.shift = (bits & 1) != 0, .control = (bits & 2) != 0, .alt = (bits & 4) != 0, .super = (bits & 8) != 0};
    }
};

} // namespace haylen::platform
