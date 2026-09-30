#include "haylen/platform/AudioStream.hpp"

#include <algorithm>
#include <stdexcept>
#include <type_traits>

namespace haylen::platform {

AudioStream::AudioStream(std::uint32_t rate, std::uint32_t count, Format value, std::size_t frames) : sampleRate(rate), channels(count), format(value), capacity(frames) {
    if (sampleRate == 0 || channels == 0 || capacity == 0) {
        throw std::invalid_argument("An audio stream needs a sample rate, channels and room for at least one frame.");
    }
    ring.resize(capacity * channels);
    history.resize(capacity * channels);
}

std::size_t AudioStream::push(std::span<const float> samples) {
    if (format != Format::Float32) {
        throw std::logic_error("This audio stream takes 16-bit samples.");
    }
    return write(samples);
}

std::size_t AudioStream::push(std::span<const std::int16_t> samples) {
    if (format != Format::Int16) {
        throw std::logic_error("This audio stream takes float samples.");
    }
    return write(samples);
}

// The history mirrors the ring, since both count the frames that fit, so it copies the converted frames from their places in the ring.
template <typename Sample> std::size_t AudioStream::write(std::span<const Sample> samples) {
    if (samples.size() % channels != 0) {
        throw std::invalid_argument("Audio samples come in whole frames, one sample for every channel.");
    }
    const std::size_t head = written.load(std::memory_order_relaxed);
    const std::size_t tail = consumed.load(std::memory_order_acquire);
    const std::size_t frames = std::min(samples.size() / channels, capacity - (head - tail));
    for (std::size_t frame = 0; frame < frames; ++frame) {
        float* slot = ring.data() + ((head + frame) % capacity) * channels;
        for (std::uint32_t channel = 0; channel < channels; ++channel) {
            const Sample sample = samples[frame * channels + channel];
            if constexpr (std::is_same_v<Sample, float>) {
                slot[channel] = sample;
            } else {
                slot[channel] = static_cast<float>(sample) / 32768.0F;
            }
        }
    }
    written.store(head + frames, std::memory_order_release);
    started.store(true, std::memory_order_relaxed);

    const std::scoped_lock lock(historyMutex);
    for (std::size_t frame = 0; frame < frames; ++frame) {
        const std::size_t offset = ((head + frame) % capacity) * channels;
        std::copy_n(ring.data() + offset, channels, history.data() + offset);
    }
    historyFrames = head + frames;
    return frames;
}

std::uint64_t AudioStream::claim() noexcept {
    return reader.fetch_add(1, std::memory_order_acq_rel) + 1;
}

void AudioStream::read(std::uint64_t token, std::span<float> output) noexcept {
    if (reader.load(std::memory_order_acquire) != token) {
        std::ranges::fill(output, 0.0F);
        return;
    }
    const std::size_t wanted = output.size() / channels;
    const std::size_t tail = consumed.load(std::memory_order_relaxed);
    const std::size_t head = written.load(std::memory_order_acquire);
    const std::size_t frames = std::min(wanted, head - tail);
    for (std::size_t frame = 0; frame < frames; ++frame) {
        std::copy_n(ring.data() + ((tail + frame) % capacity) * channels, channels, output.data() + frame * channels);
    }
    std::fill(output.begin() + static_cast<std::ptrdiff_t>(frames * channels), output.end(), 0.0F);
    consumed.store(tail + frames, std::memory_order_release);
    if (frames < wanted && started.load(std::memory_order_relaxed)) {
        underruns.fetch_add(1, std::memory_order_relaxed);
    }
}

std::size_t AudioStream::copyLatest(std::span<float> destination) const {
    const std::scoped_lock lock(historyMutex);
    const std::size_t frames = std::min({destination.size() / channels, historyFrames, capacity});
    const std::size_t first = historyFrames - frames;
    for (std::size_t frame = 0; frame < frames; ++frame) {
        std::copy_n(history.data() + ((first + frame) % capacity) * channels, channels, destination.data() + frame * channels);
    }
    return frames * channels;
}

// The reader only moves `consumed` forward and never past `written`, so reading `consumed` first never sees more frames consumed than written.
std::size_t AudioStream::getBufferedFrames() const noexcept {
    const std::size_t tail = consumed.load(std::memory_order_acquire);
    return written.load(std::memory_order_acquire) - tail;
}

} // namespace haylen::platform
