#include "haylen/audio/Limiter.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace haylen::audio {

Limiter::Limiter(const Settings& settings) : threshold(settings.threshold), release(settings.release) {
    requireThreshold(settings.threshold);
    requireRelease(settings.release);
}

void Limiter::setThreshold(float value) {
    requireThreshold(value);
    threshold.store(value, std::memory_order_relaxed);
}

void Limiter::setRelease(float value) {
    requireRelease(value);
    release.store(value, std::memory_order_relaxed);
}

void Limiter::requireThreshold(float value) {
    if (!(value > 0.0F && value <= 1.0F)) {
        throw std::invalid_argument("A limiter threshold must be above 0 and at most 1.");
    }
}

void Limiter::requireRelease(float value) {
    if (!(value >= 0.0F && value <= 10.0F)) {
        throw std::invalid_argument("A limiter release must be from 0 to 10 seconds.");
    }
}

void Limiter::prepare(std::uint32_t rate, std::uint32_t count) {
    sampleRate = static_cast<float>(rate);
    channels = count;
    gain = 1.0F;
    gainOut.store(1.0F, std::memory_order_relaxed);
}

// The gain of a frame never lets its loudest channel pass the threshold, so the output stays under it whatever the input, and between peaks it recovers by a share of the way to 1 on every frame.
void Limiter::process(const float* input, float* output, std::uint32_t frames) noexcept {
    const float level = getThreshold();
    const float seconds = getRelease();
    const float recovery = seconds > 0.0F ? 1.0F - std::exp(-1.0F / (seconds * sampleRate)) : 1.0F;
    for (std::uint32_t frame = 0; frame < frames; ++frame) {
        const float* in = input + static_cast<std::size_t>(frame) * channels;
        float peak = 0.0F;
        for (std::uint32_t channel = 0; channel < channels; ++channel) {
            peak = std::max(peak, std::abs(in[channel]));
        }
        gain += (1.0F - gain) * recovery;
        if (peak * gain > level) {
            gain = level / peak;
        }
        float* out = output + static_cast<std::size_t>(frame) * channels;
        for (std::uint32_t channel = 0; channel < channels; ++channel) {
            out[channel] = in[channel] * gain;
        }
    }
    gainOut.store(gain, std::memory_order_relaxed);
}

} // namespace haylen::audio
