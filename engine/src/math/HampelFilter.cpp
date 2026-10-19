#include "haylen/math/HampelFilter.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <string>

#include "haylen/math/MedianFilter.hpp"

namespace haylen::math {

HampelFilter::HampelFilter(int size, float limit, int count) : window(size), threshold(limit), dimensions(count) {
    if (size < 1 || size > kMaxWindow) {
        throw std::invalid_argument("The window of a filter holds from 1 to " + std::to_string(kMaxWindow) + " updates, not " + std::to_string(size) + ".");
    }
    if (count < 1 || count > kMaxDimensions) {
        throw std::invalid_argument("A filter has from 1 to " + std::to_string(kMaxDimensions) + " dimensions, not " + std::to_string(count) + ".");
    }
    if (!(limit > 0.0F) || !std::isfinite(limit)) {
        throw std::invalid_argument("The threshold of a Hampel filter is a positive number of spreads.");
    }
    history.resize(static_cast<std::size_t>(size * count));
    scratch.resize(static_cast<std::size_t>(size));
}

// The window holds the new value too, so a value only counts as an outlier against the values around it, and the outlier itself stays in the window as the value that arrived.
void HampelFilter::update(std::span<float> values) noexcept {
    const auto count = static_cast<std::size_t>(dimensions);
    const std::size_t used = std::min(values.size(), count);
    for (std::size_t index = 0; index < used; ++index) {
        history[next * count + index] = values[index];
    }
    next = (next + 1) % static_cast<std::size_t>(window);
    filled = std::min(filled + 1, static_cast<std::size_t>(window));

    outlier = false;
    const std::span<float> slots = std::span(scratch).first(filled);
    for (std::size_t index = 0; index < used; ++index) {
        for (std::size_t slot = 0; slot < filled; ++slot) {
            slots[slot] = history[slot * count + index];
        }
        const float center = MedianFilter::median(slots);
        for (std::size_t slot = 0; slot < filled; ++slot) {
            slots[slot] = std::abs(history[slot * count + index] - center);
        }
        const float spread = kSpreadScale * MedianFilter::median(slots);
        if (std::abs(values[index] - center) > threshold * spread) {
            values[index] = center;
            outlier = true;
        }
    }
}

float HampelFilter::update(float value) noexcept {
    update(std::span(&value, 1));
    return value;
}

void HampelFilter::reset() noexcept {
    next = 0;
    filled = 0;
    outlier = false;
}

} // namespace haylen::math
