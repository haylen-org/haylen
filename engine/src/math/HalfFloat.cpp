#include "math/HalfFloat.hpp"

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstdint>

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

// A subnormal half counts steps of the smallest one, and a normal half moves its exponent and fraction into the bits of a float.
float HalfFloat::toFloat(std::uint16_t bits) noexcept {
    if (bits < kFirstNormal) {
        return static_cast<float>(bits) / kSubnormalSteps;
    }

    return std::bit_cast<float>((static_cast<std::uint32_t>(bits) << 13U) + 0x38000000U);
}

} // namespace haylen::math
