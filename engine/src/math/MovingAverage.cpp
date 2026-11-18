#include "haylen/math/MovingAverage.hpp"

#include <algorithm>

#include "haylen/math/SignalFilterHelper.hpp"

namespace haylen::math {

MovingAverage::MovingAverage(int size, int count) : window(size), dimensions(count) {
    SignalFilterHelper::checkWindow(size, kMaxWindow);
    SignalFilterHelper::checkDimensions(count);

    history.resize(static_cast<std::size_t>(size * count));
}

// A running sum adds the new value and takes away the value that leaves the window, so an update costs the same for any window.
void MovingAverage::update(std::span<float> values) {
    const auto count = static_cast<std::size_t>(dimensions);
    const std::size_t used = std::min(values.size(), count);
    SignalFilterHelper::checkValues(values.first(used));

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

float MovingAverage::update(float value) {
    update(std::span(&value, 1));
    return value;
}

void MovingAverage::reset() noexcept {
    sums.fill(0.0);
    next = 0;
    filled = 0;
}

} // namespace haylen::math
