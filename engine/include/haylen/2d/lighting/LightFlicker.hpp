#pragma once

#include <cstdint>

namespace haylen::lighting2d {

// Flame-like wavering of light intensity.
class LightFlicker final {
  public:
    // Returns a light intensity multiplier between 1 - amount and 1, so it can scale any valid light intensity. The same seed and time always give the same value.
    [[nodiscard]] static float intensity(float time, float speed, float amount, std::uint64_t seed) noexcept;
};

} // namespace haylen::lighting2d
