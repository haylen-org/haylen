#pragma once

#include <cstdint>

namespace haylen::math {

// The bits of half floats, which images and vertex data of the GPU hold where a byte is too coarse and a float too large.
class HalfFloat final {
  public:
    // The largest finite half float, the smallest normal one and the steps of the subnormal halves below it.
    static constexpr float kLargest = 65504.0F;
    static constexpr float kSmallestNormal = 6.103515625e-05F;
    static constexpr float kSubnormalSteps = 16777216.0F;

    HalfFloat() = delete;

    // Converts a value of zero or more to the bits of a half float, rounding to the nearest and keeping it within the largest finite half.
    [[nodiscard]] static std::uint16_t fromFloat(float value) noexcept;
};

} // namespace haylen::math
