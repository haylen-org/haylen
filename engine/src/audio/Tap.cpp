#include "haylen/audio/Tap.hpp"

#include <algorithm>

#include "audio/TapSet.hpp"

namespace haylen::audio {

Tap::Tap(Passkey, TapSet& owner, std::uint32_t rate, std::uint32_t count, std::size_t frames) : sampleRate(rate), channels(count), capacity(frames), set(&owner) {
    ring.resize(capacity * channels);
}

std::size_t Tap::read(std::span<float> samples) noexcept {
    const std::size_t tail = consumed.load(std::memory_order_relaxed);
    const std::size_t head = written.load(std::memory_order_acquire);
    const std::size_t frames = std::min(samples.size() / channels, head - tail);
    for (std::size_t frame = 0; frame < frames; ++frame) {
        std::copy_n(ring.data() + ((tail + frame) % capacity) * channels, channels, samples.data() + frame * channels);
    }

    consumed.store(tail + frames, std::memory_order_release);
    return frames;
}

// The reader only moves `consumed` forward and never past `written`, so reading `consumed` first never sees more frames read than written.
std::size_t Tap::getAvailableFrames() const noexcept {
    const std::size_t tail = consumed.load(std::memory_order_acquire);
    return written.load(std::memory_order_acquire) - tail;
}

void Tap::close() {
    if (set != nullptr) {
        set->close(*this);
    }
}

void Tap::write(const float* samples, std::size_t frames) noexcept {
    const std::size_t head = written.load(std::memory_order_relaxed);
    const std::size_t tail = consumed.load(std::memory_order_acquire);
    const std::size_t fitting = std::min(frames, capacity - (head - tail));
    for (std::size_t frame = 0; frame < fitting; ++frame) {
        std::copy_n(samples + frame * channels, channels, ring.data() + ((head + frame) % capacity) * channels);
    }

    written.store(head + fitting, std::memory_order_release);
    if (fitting < frames) {
        dropped.fetch_add(frames - fitting, std::memory_order_relaxed);
    }
}

} // namespace haylen::audio
