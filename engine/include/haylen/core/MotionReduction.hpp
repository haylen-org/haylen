#pragma once

#include <cstdint>

namespace haylen::core {

// What a tween or a scene transition does while the system asks for reduced motion: it keeps playing as it is, plays in a quarter of its time, or skips to its end.
enum class MotionReduction : std::uint8_t {
    Keep,
    Shorten,
    Skip,
};

} // namespace haylen::core
