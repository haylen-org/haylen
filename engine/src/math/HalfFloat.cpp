#include "math/HalfFloat.hpp"

#include <algorithm>
#include <bit>
#include <cmath>

namespace haylen::math {

std::uint16_t HalfFloat::fromFloat(float value) noexcept {
    const float clamped = std::clamp(value, 0.0F, kLargest);
    if (clamped < kSmallestNormal) {
        return static_cast<std::uint16_t>(std::lround(clamped * kSubnormalSteps));
    }
    const auto bits = std::bit_cast<std::uint32_t>(clamped);
    const std::uint32_t rounded = bits + 0x0FFFU + ((bits >> 13U) & 1U);
    return static_cast<std::uint16_t>((rounded - 0x38000000U) >> 13U);
}

} // namespace haylen::math
