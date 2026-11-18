#pragma once

#include <array>
#include <span>

#include "haylen/math/SignalFilterHelper.hpp"

namespace haylen::math {

// Smooths up to four values, such as a position or the readings of a sensor, by letting changes slower than a cutoff frequency through and damping faster ones. The cutoff is in hertz and every update takes the seconds since the last one, so the smoothing is the same at any frame rate. The first update takes its values as they are.
class LowPassFilter final {
  public:
    // Throws `std::invalid_argument` for a cutoff that is not positive or for dimensions out of 1 to `SignalFilterHelper::kMaxDimensions`.
    LowPassFilter(float cutoff, int dimensions);

    // Filters the values in place, one for each dimension of the filter. Throws `std::invalid_argument` for a value that is not finite, which leaves the filter as it was.
    void update(float seconds, std::span<float> values);
    [[nodiscard]] float update(float seconds, float value);

    [[nodiscard]] float getCutoff() const noexcept {
        return cutoff;
    }
    void setCutoff(float value);
    [[nodiscard]] int getDimensions() const noexcept {
        return dimensions;
    }
    void reset() noexcept;

  private:
    float cutoff;
    int dimensions;
    bool started = false;
    std::array<float, SignalFilterHelper::kMaxDimensions> state{};
};

} // namespace haylen::math
