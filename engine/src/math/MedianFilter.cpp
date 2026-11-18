#include "haylen/math/MedianFilter.hpp"

#include <algorithm>

#include "haylen/math/SignalFilterHelper.hpp"

namespace haylen::math {

MedianFilter::MedianFilter(int size, int count) : window(size), dimensions(count) {
    SignalFilterHelper::checkWindow(size, kMaxWindow);
    SignalFilterHelper::checkDimensions(count);

    history.resize(static_cast<std::size_t>(size * count));
    scratch.resize(static_cast<std::size_t>(size));
}

void MedianFilter::update(std::span<float> values) {
    const auto count = static_cast<std::size_t>(dimensions);
    const std::size_t used = std::min(values.size(), count);
    SignalFilterHelper::checkValues(values.first(used));

    for (std::size_t index = 0; index < used; ++index) {
        history[next * count + index] = values[index];
    }

    next = (next + 1) % static_cast<std::size_t>(window);
    filled = std::min(filled + 1, static_cast<std::size_t>(window));
    for (std::size_t index = 0; index < used; ++index) {
        for (std::size_t slot = 0; slot < filled; ++slot) {
            scratch[slot] = history[slot * count + index];
        }

        values[index] = median(std::span(scratch).first(filled));
    }
}

float MedianFilter::update(float value) {
    update(std::span(&value, 1));
    return value;
}

void MedianFilter::reset() noexcept {
    next = 0;
    filled = 0;
}

float MedianFilter::median(std::span<float> values) noexcept {
    if (values.empty()) {
        return 0.0F;
    }

    const std::size_t middle = values.size() / 2;
    std::ranges::nth_element(values, values.begin() + static_cast<std::ptrdiff_t>(middle));
    const float upper = values[middle];
    if (values.size() % 2 == 1) {
        return upper;
    }

    return (*std::ranges::max_element(values.first(middle)) + upper) * 0.5F;
}

} // namespace haylen::math
