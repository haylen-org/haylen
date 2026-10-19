#pragma once

#include <array>
#include <span>

namespace haylen::math {

// Estimates up to four values that drift, such as the position of a tracked finger or the reading of a noisy sensor, from measurements of known noise. Each update weighs the measurement against the estimate by their uncertainties, so the estimate follows real changes and ignores most of the noise. A larger process noise follows changes faster, and a larger measurement noise smooths more. The first update takes its values as they are.
class KalmanFilter final {
  public:
    static constexpr int kMaxDimensions = 4;

    // Throws `std::invalid_argument` for a noise that is not positive or for dimensions out of 1 to `kMaxDimensions`.
    KalmanFilter(float processNoise, float measurementNoise, int dimensions);

    // Filters the values in place, one for each dimension of the filter.
    void update(std::span<float> values) noexcept;
    [[nodiscard]] float update(float value) noexcept;

    // How sure the filter is of its estimate of the first value, the variance that falls toward a floor as measurements arrive.
    [[nodiscard]] float getUncertainty() const noexcept {
        return covariance[0];
    }
    [[nodiscard]] int getDimensions() const noexcept {
        return dimensions;
    }
    void reset() noexcept;

  private:
    float processNoise;
    float measurementNoise;
    int dimensions;
    bool started = false;
    std::array<float, kMaxDimensions> estimate{};
    std::array<float, kMaxDimensions> covariance{};
};

} // namespace haylen::math
