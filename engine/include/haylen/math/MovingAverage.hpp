#pragma once

#include <array>
#include <cstddef>
#include <span>
#include <vector>

#include "haylen/math/SignalFilterHelper.hpp"

namespace haylen::math {

// Smooths up to four values with the average of the last updates in a window, which follows a steady change with a delay of half the window. Until the window fills, the average is of the updates so far.
class MovingAverage final {
  public:
    static constexpr int kMaxWindow = 1024;

    // Throws `std::invalid_argument` for a window out of 1 to `kMaxWindow` or dimensions out of 1 to `SignalFilterHelper::kMaxDimensions`.
    MovingAverage(int window, int dimensions);

    // Filters the values in place, one for each dimension of the filter. Throws `std::invalid_argument` for a value that is not finite, which leaves the filter as it was.
    void update(std::span<float> values);
    [[nodiscard]] float update(float value);

    [[nodiscard]] int getWindow() const noexcept {
        return window;
    }
    [[nodiscard]] int getDimensions() const noexcept {
        return dimensions;
    }
    void reset() noexcept;

  private:
    int window;
    int dimensions;
    std::vector<float> history;
    std::array<double, SignalFilterHelper::kMaxDimensions> sums{};
    std::size_t next = 0;
    std::size_t filled = 0;
};

} // namespace haylen::math
