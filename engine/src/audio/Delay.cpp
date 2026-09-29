#include "haylen/audio/Delay.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <string>

namespace haylen::audio {

Delay::Delay(const Settings& settings) : maxTime(settings.maxTime), time(settings.time), feedback(settings.feedback), wet(settings.wet), dry(settings.dry) {
    if (!(settings.maxTime > 0.0F) || settings.maxTime > kLongestTime) {
        throw std::invalid_argument("A delay needs a largest time above 0 and up to 10 seconds.");
    }
    requireTime(settings.time);
    requireFeedback(settings.feedback);
    requireLevel(settings.wet, "wet");
    requireLevel(settings.dry, "dry");
}

void Delay::setTime(float value) {
    requireTime(value);
    time.store(value, std::memory_order_relaxed);
}

void Delay::setFeedback(float value) {
    requireFeedback(value);
    feedback.store(value, std::memory_order_relaxed);
}

void Delay::setWet(float value) {
    requireLevel(value, "wet");
    wet.store(value, std::memory_order_relaxed);
}

void Delay::setDry(float value) {
    requireLevel(value, "dry");
    dry.store(value, std::memory_order_relaxed);
}

float Delay::getTail() const noexcept {
    const float repeat = getTime();
    const float level = getFeedback();
    return level > 0.0F ? repeat * (1.0F + std::log(kSilence) / std::log(level)) : repeat;
}

void Delay::requireTime(float value) const {
    if (!(value >= 0.0F && value <= maxTime)) {
        throw std::invalid_argument("A delay time must be between 0 and the largest time of the delay.");
    }
}

void Delay::requireFeedback(float value) {
    if (!(value >= 0.0F && value < 1.0F)) {
        throw std::invalid_argument("A delay feedback must be at least 0 and below 1.");
    }
}

void Delay::requireLevel(float value, const char* name) {
    if (!(value >= 0.0F) || !std::isfinite(value)) {
        throw std::invalid_argument(std::string("A delay ") + name + " level must be 0 or more.");
    }
}

void Delay::prepare(std::uint32_t rate, std::uint32_t count) {
    sampleRate = static_cast<float>(rate);
    channels = count;
    ringFrames = static_cast<std::size_t>(std::ceil(maxTime * sampleRate)) + 2;
    ring.assign(ringFrames * channels, 0.0F);
    writeFrame = 0;
    currentDelay = -1.0;
}

void Delay::process(const float* input, float* output, std::uint32_t frames) noexcept {
    // The delay glides from where the last block ended to the current time, so a change of time bends the pitch of the repeats instead of clicking.
    const double target = std::clamp(static_cast<double>(getTime() * sampleRate), 1.0, static_cast<double>(ringFrames - 2));
    const double start = currentDelay < 0.0 ? target : currentDelay;
    const double step = (target - start) / static_cast<double>(std::max<std::uint32_t>(frames, 1));
    const float repeat = getFeedback();
    const float wetLevel = getWet();
    const float dryLevel = getDry();

    for (std::uint32_t frame = 0; frame < frames; ++frame) {
        const double delay = start + step * static_cast<double>(frame + 1);
        const double position = static_cast<double>(writeFrame + ringFrames) - delay;
        const auto whole = static_cast<std::size_t>(position);
        const auto fraction = static_cast<float>(position - static_cast<double>(whole));
        const std::size_t older = (whole % ringFrames) * channels;
        const std::size_t newer = ((whole + 1) % ringFrames) * channels;
        const std::size_t written = writeFrame * channels;
        const std::size_t sample = static_cast<std::size_t>(frame) * channels;
        for (std::uint32_t channel = 0; channel < channels; ++channel) {
            const float delayed = ring[older + channel] + (ring[newer + channel] - ring[older + channel]) * fraction;
            const float in = input[sample + channel];
            ring[written + channel] = in + delayed * repeat;
            output[sample + channel] = in * dryLevel + delayed * wetLevel;
        }
        writeFrame = (writeFrame + 1) % ringFrames;
    }
    currentDelay = target;
}

} // namespace haylen::audio
