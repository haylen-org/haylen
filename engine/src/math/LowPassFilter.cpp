#include "haylen/math/LowPassFilter.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <numbers>
#include <stdexcept>
#include <string>

namespace haylen::math {

LowPassFilter::LowPassFilter(float cutoffFrequency, int count) : cutoff(cutoffFrequency), dimensions(count) {
    setCutoff(cutoffFrequency);
    if (count < 1 || count > kMaxDimensions) {
        throw std::invalid_argument("A filter has from 1 to " + std::to_string(kMaxDimensions) + " dimensions, not " + std::to_string(count) + ".");
    }
}

void LowPassFilter::setCutoff(float value) {
    if (!(value > 0.0F) || !std::isfinite(value)) {
        throw std::invalid_argument("The cutoff of a low-pass filter is a positive number of hertz.");
    }
    cutoff = value;
}

// The share of the way toward the new values follows from the time constant of the cutoff, so two updates of half the time move as far as one of the whole time.
void LowPassFilter::update(float seconds, std::span<float> values) noexcept {
    const std::size_t count = std::min(values.size(), static_cast<std::size_t>(dimensions));
    if (!started) {
        started = true;
        for (std::size_t index = 0; index < count; ++index) {
            state[index] = values[index];
        }
        return;
    }
    const float share = 1.0F - std::exp(-2.0F * std::numbers::pi_v<float> * cutoff * std::max(seconds, 0.0F));
    for (std::size_t index = 0; index < count; ++index) {
        state[index] += (values[index] - state[index]) * share;
        values[index] = state[index];
    }
}

float LowPassFilter::update(float seconds, float value) noexcept {
    update(seconds, std::span(&value, 1));
    return value;
}

void LowPassFilter::reset() noexcept {
    started = false;
}

} // namespace haylen::math
