#include "haylen/audio/Filter.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <stdexcept>

namespace haylen::audio {

Filter::Filter(Kind value, const Settings& settings) : kind(value), cutoff(settings.cutoff), q(settings.q), gain(settings.gain) {
    requireCutoff(settings.cutoff);
    requireQ(settings.q);
    requireGain(settings.gain);
    if (settings.gain != 0.0F && !hasGain()) {
        throw std::invalid_argument("Only peak and shelf filters have a gain.");
    }
}

void Filter::setCutoff(float value) {
    requireCutoff(value);
    cutoff.store(value, std::memory_order_relaxed);
    changed();
}

void Filter::setQ(float value) {
    requireQ(value);
    q.store(value, std::memory_order_relaxed);
    changed();
}

void Filter::setGain(float value) {
    if (!hasGain()) {
        throw std::invalid_argument("Only peak and shelf filters have a gain.");
    }
    requireGain(value);
    gain.store(value, std::memory_order_relaxed);
    changed();
}

bool Filter::hasGain() const noexcept {
    return kind == Kind::Peak || kind == Kind::LowShelf || kind == Kind::HighShelf;
}

void Filter::requireCutoff(float value) {
    if (!(value > 0.0F) || !std::isfinite(value)) {
        throw std::invalid_argument("A filter cutoff must be a positive frequency.");
    }
}

void Filter::requireQ(float value) {
    if (!(value > 0.0F) || !std::isfinite(value)) {
        throw std::invalid_argument("A filter q must be positive.");
    }
}

void Filter::requireGain(float value) {
    if (!std::isfinite(value)) {
        throw std::invalid_argument("A filter gain must be a finite number of decibels.");
    }
}

void Filter::changed() noexcept {
    version.fetch_add(1, std::memory_order_release);
}

Filter::Coefficients Filter::design(Kind type, float rate, float frequency, float quality, float decibels) noexcept {
    // The bilinear transform needs a frequency below half the sample rate.
    const double corner = std::min(static_cast<double>(frequency), 0.49 * static_cast<double>(rate));
    const double omega = 2.0 * std::numbers::pi * corner / static_cast<double>(rate);
    const double cosine = std::cos(omega);
    const double alpha = std::sin(omega) / (2.0 * static_cast<double>(quality));
    const double amplitude = std::pow(10.0, static_cast<double>(decibels) / 40.0);
    const double shelf = 2.0 * std::sqrt(amplitude) * alpha;

    double b0 = 1.0;
    double b1 = 0.0;
    double b2 = 0.0;
    double a0 = 1.0;
    double a1 = 0.0;
    double a2 = 0.0;
    switch (type) {
    case Kind::Lowpass:
        b0 = (1.0 - cosine) / 2.0;
        b1 = 1.0 - cosine;
        b2 = b0;
        a0 = 1.0 + alpha;
        a1 = -2.0 * cosine;
        a2 = 1.0 - alpha;
        break;
    case Kind::Highpass:
        b0 = (1.0 + cosine) / 2.0;
        b1 = -(1.0 + cosine);
        b2 = b0;
        a0 = 1.0 + alpha;
        a1 = -2.0 * cosine;
        a2 = 1.0 - alpha;
        break;
    case Kind::Bandpass:
        b0 = alpha;
        b2 = -alpha;
        a0 = 1.0 + alpha;
        a1 = -2.0 * cosine;
        a2 = 1.0 - alpha;
        break;
    case Kind::Notch:
        b1 = -2.0 * cosine;
        b2 = 1.0;
        a0 = 1.0 + alpha;
        a1 = -2.0 * cosine;
        a2 = 1.0 - alpha;
        break;
    case Kind::Peak:
        b0 = 1.0 + alpha * amplitude;
        b1 = -2.0 * cosine;
        b2 = 1.0 - alpha * amplitude;
        a0 = 1.0 + alpha / amplitude;
        a1 = -2.0 * cosine;
        a2 = 1.0 - alpha / amplitude;
        break;
    case Kind::LowShelf:
        b0 = amplitude * ((amplitude + 1.0) - (amplitude - 1.0) * cosine + shelf);
        b1 = 2.0 * amplitude * ((amplitude - 1.0) - (amplitude + 1.0) * cosine);
        b2 = amplitude * ((amplitude + 1.0) - (amplitude - 1.0) * cosine - shelf);
        a0 = (amplitude + 1.0) + (amplitude - 1.0) * cosine + shelf;
        a1 = -2.0 * ((amplitude - 1.0) + (amplitude + 1.0) * cosine);
        a2 = (amplitude + 1.0) + (amplitude - 1.0) * cosine - shelf;
        break;
    case Kind::HighShelf:
        b0 = amplitude * ((amplitude + 1.0) + (amplitude - 1.0) * cosine + shelf);
        b1 = -2.0 * amplitude * ((amplitude - 1.0) + (amplitude + 1.0) * cosine);
        b2 = amplitude * ((amplitude + 1.0) + (amplitude - 1.0) * cosine - shelf);
        a0 = (amplitude + 1.0) - (amplitude - 1.0) * cosine + shelf;
        a1 = 2.0 * ((amplitude - 1.0) - (amplitude + 1.0) * cosine);
        a2 = (amplitude + 1.0) - (amplitude - 1.0) * cosine - shelf;
        break;
    }
    return {static_cast<float>(b0 / a0), static_cast<float>(b1 / a0), static_cast<float>(b2 / a0), static_cast<float>(a1 / a0), static_cast<float>(a2 / a0)};
}

void Filter::prepare(std::uint32_t rate, std::uint32_t count) {
    sampleRate = static_cast<float>(rate);
    channels = count;
    history.assign(static_cast<std::size_t>(count) * 2, 0.0F);
}

void Filter::process(const float* input, float* output, std::uint32_t frames) noexcept {
    const std::uint32_t current = version.load(std::memory_order_acquire);
    if (current != appliedVersion) {
        appliedVersion = current;
        coefficients = design(kind, sampleRate, getCutoff(), getQ(), getGain());
    }

    // Transposed direct form II, with the two delay elements of each channel side by side.
    const auto [b0, b1, b2, a1, a2] = coefficients;
    for (std::uint32_t frame = 0; frame < frames; ++frame) {
        for (std::uint32_t channel = 0; channel < channels; ++channel) {
            const std::size_t sample = static_cast<std::size_t>(frame) * channels + channel;
            float* delays = history.data() + static_cast<std::size_t>(channel) * 2;
            const float in = input[sample];
            const float out = b0 * in + delays[0];
            delays[0] = b1 * in - a1 * out + delays[1];
            delays[1] = b2 * in - a2 * out;
            output[sample] = out;
        }
    }
}

} // namespace haylen::audio
