#pragma once

#include <span>

namespace haylen::math {

// The limits and the checks that the signal filters share.
class SignalFilterHelper final {
  public:
    // The most values that one filter smooths together.
    static constexpr int kMaxDimensions = 4;

    // Throws `std::invalid_argument` for dimensions out of 1 to `kMaxDimensions`.
    static void checkDimensions(int dimensions);

    // Throws `std::invalid_argument` for a window out of 1 to `maximum` updates.
    static void checkWindow(int window, int maximum);

    // Throws `std::invalid_argument` for a value that is not finite, which would spoil every later result of a filter.
    static void checkValues(std::span<const float> values);
};

} // namespace haylen::math
