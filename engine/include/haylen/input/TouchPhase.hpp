#pragma once

#include <cstdint>

namespace haylen::input {

enum class TouchPhase : std::uint8_t {
    Began,
    Moved,
    Stationary,
    Ended,
    Cancelled,
};

} // namespace haylen::input
