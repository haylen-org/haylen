#pragma once

#include <cstddef>
#include <span>
#include <vector>

namespace haylen::math {

// Lets up to four values through unchanged until one of them is an outlier, a value farther from the median of the last updates in a window than the threshold times their spread, measured by the median of their distances to the median. An outlier takes the median instead, so single spikes vanish while every other value stays exact.
class HampelFilter final {
  public:
    static constexpr int kMaxDimensions = 4;
    static constexpr int kMaxWindow = 255;

    // Throws `std::invalid_argument` for a window out of 1 to `kMaxWindow`, dimensions out of 1 to `kMaxDimensions` or a threshold that is not positive.
    HampelFilter(int window, float threshold, int dimensions);

    // Filters the values in place, one for each dimension of the filter.
    void update(std::span<float> values) noexcept;
    [[nodiscard]] float update(float value) noexcept;

    // Whether the last update replaced a value as an outlier.
    [[nodiscard]] bool isOutlier() const noexcept {
        return outlier;
    }
    [[nodiscard]] int getWindow() const noexcept {
        return window;
    }
    [[nodiscard]] float getThreshold() const noexcept {
        return threshold;
    }
    [[nodiscard]] int getDimensions() const noexcept {
        return dimensions;
    }
    void reset() noexcept;

  private:
    // The spread of normally distributed values from the median of their distances to their median.
    static constexpr float kSpreadScale = 1.4826F;

    int window;
    float threshold;
    int dimensions;
    std::vector<float> history;
    std::vector<float> scratch;
    std::size_t next = 0;
    std::size_t filled = 0;
    bool outlier = false;
};

} // namespace haylen::math
