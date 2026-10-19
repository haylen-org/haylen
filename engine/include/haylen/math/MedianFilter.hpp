#pragma once

#include <cstddef>
#include <span>
#include <vector>

namespace haylen::math {

// Smooths up to four values with the median of the last updates in a window, which keeps steps sharp and drops single spikes, such as a sensor that misreads once. Until the window fills, the median is of the updates so far.
class MedianFilter final {
  public:
    static constexpr int kMaxDimensions = 4;
    static constexpr int kMaxWindow = 255;

    // Throws `std::invalid_argument` for a window out of 1 to `kMaxWindow` or dimensions out of 1 to `kMaxDimensions`.
    MedianFilter(int window, int dimensions);

    // Filters the values in place, one for each dimension of the filter.
    void update(std::span<float> values) noexcept;
    [[nodiscard]] float update(float value) noexcept;

    [[nodiscard]] int getWindow() const noexcept {
        return window;
    }
    [[nodiscard]] int getDimensions() const noexcept {
        return dimensions;
    }
    void reset() noexcept;

    // The median of the values, the mean of the two middle ones for an even count, which reorders them. Returns 0 for no values.
    [[nodiscard]] static float median(std::span<float> values) noexcept;

  private:
    int window;
    int dimensions;
    std::vector<float> history;
    std::vector<float> scratch;
    std::size_t next = 0;
    std::size_t filled = 0;
};

} // namespace haylen::math
