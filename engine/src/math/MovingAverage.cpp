#include "haylen/math/MovingAverage.hpp"

#include <algorithm>
#include <stdexcept>
#include <string>

namespace haylen::math {

MovingAverage::MovingAverage(int size, int count) : window(size), dimensions(count) {
    if (size < 1 || size > kMaxWindow) {
        throw std::invalid_argument("The window of a filter holds from 1 to " + std::to_string(kMaxWindow) + " updates, not " + std::to_string(size) + ".");
    }
    if (count < 1 || count > kMaxDimensions) {
        throw std::invalid_argument("A filter has from 1 to " + std::to_string(kMaxDimensions) + " dimensions, not " + std::to_string(count) + ".");
    }
    history.resize(static_cast<std::size_t>(size * count));
}

// A running sum adds the new value and takes away the value that leaves the window, so an update costs the same for any window.
void MovingAverage::update(std::span<float> values) noexcept {
    const auto count = static_cast<std::size_t>(dimensions);
    const std::size_t used = std::min(values.size(), count);
    for (std::size_t index = 0; index < used; ++index) {
        float& slot = history[next * count + index];
        if (filled == static_cast<std::size_t>(window)) {
            sums[index] -= slot;
        }
        slot = values[index];
        sums[index] += slot;
    }
    next = (next + 1) % static_cast<std::size_t>(window);
    filled = std::min(filled + 1, static_cast<std::size_t>(window));
    for (std::size_t index = 0; index < used; ++index) {
        values[index] = static_cast<float>(sums[index] / static_cast<double>(filled));
    }
}

float MovingAverage::update(float value) noexcept {
    update(std::span(&value, 1));
    return value;
}

void MovingAverage::reset() noexcept {
    sums.fill(0.0);
    next = 0;
    filled = 0;
}

} // namespace haylen::math
