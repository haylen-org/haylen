#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace haylen::audio {

class TapSet;

// A tap on the master bus of a mixer, which hears each block of the mix as it leaves for the output, at the sample rate and in the channels of the mixer, such as to record the sound of the app or to send it to a plugin. The thread that mixes writes into a lock-free ring that the tap allocates once, and one thread at a time reads it, the frame thread or a thread of a plugin. When the reader falls behind, the frames that do not fit are dropped and counted, so the mix never waits for a tap. The mixer feeds the tap until `close` or until nothing but the mixer holds it, and a closed tap keeps the frames that wait in it.
class Tap final {
    struct Passkey {
        explicit Passkey() = default;
    };

  public:
    Tap(Passkey, TapSet& owner, std::uint32_t rate, std::uint32_t count, std::size_t frames);

    Tap(const Tap&) = delete;
    Tap& operator=(const Tap&) = delete;

    [[nodiscard]] std::uint32_t getSampleRate() const noexcept {
        return sampleRate;
    }
    [[nodiscard]] std::uint32_t getChannels() const noexcept {
        return channels;
    }
    [[nodiscard]] std::size_t getCapacity() const noexcept {
        return capacity;
    }

    // Moves the oldest frames that wait into the samples, interleaved and in whole frames, and returns how many frames it moved.
    std::size_t read(std::span<float> samples) noexcept;

    // The frames that wait for the reader.
    [[nodiscard]] std::size_t getAvailableFrames() const noexcept;

    // The frames that the mix dropped because the ring was full.
    [[nodiscard]] std::uint64_t getDroppedFrames() const noexcept {
        return dropped.load(std::memory_order_relaxed);
    }

    // Whether the mixer still feeds the tap.
    [[nodiscard]] bool isOpen() const noexcept {
        return open.load(std::memory_order_acquire);
    }

    // Stops feeding the tap, on the frame thread. Closing a closed tap does nothing.
    void close();

  private:
    friend class TapSet;

    // Writes interleaved frames on the thread that mixes, and drops the frames that do not fit.
    void write(const float* samples, std::size_t frames) noexcept;

    const std::uint32_t sampleRate;
    const std::uint32_t channels;
    const std::size_t capacity;

    // The ring holds `capacity` frames. The frame counters only grow, the mix advances `written` and the reader advances `consumed`, and each reads the other with acquire order.
    std::vector<float> ring;
    std::atomic<std::size_t> written{0};
    std::atomic<std::size_t> consumed{0};
    std::atomic<std::uint64_t> dropped{0};
    std::atomic<bool> open{true};

    // The taps of the mixer that feed this one, on the frame thread, and null once the tap closed.
    TapSet* set;
};

} // namespace haylen::audio
