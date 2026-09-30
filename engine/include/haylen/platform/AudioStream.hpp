#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <span>
#include <vector>

namespace haylen::platform {

// Audio that native code, such as a microphone, a synthesized voice or decoded network audio, pushes from any thread into a lock-free ring, which the mixer plays as a voice. One thread at a time pushes, and the voice that played the stream last reads it on the mixing thread, which resamples it to the rate of the mixer. The app also reads the newest samples for analysis, such as a level meter.
class AudioStream final {
  public:
    enum class Format : std::uint8_t {
        Float32,
        Int16,
    };

    // Throws std::invalid_argument for a sample rate, channels or capacity of 0.
    AudioStream(std::uint32_t sampleRate, std::uint32_t channels, Format format, std::size_t capacityFrames);

    AudioStream(const AudioStream&) = delete;
    AudioStream& operator=(const AudioStream&) = delete;

    [[nodiscard]] std::uint32_t getSampleRate() const noexcept {
        return sampleRate;
    }
    [[nodiscard]] std::uint32_t getChannels() const noexcept {
        return channels;
    }
    [[nodiscard]] Format getFormat() const noexcept {
        return format;
    }
    [[nodiscard]] std::size_t getCapacity() const noexcept {
        return capacity;
    }

    // Writes interleaved samples in the format of the stream and returns how many frames fit, dropping the rest while the ring is full. Throws std::logic_error for samples of the other format and std::invalid_argument for samples that do not fill whole frames.
    std::size_t push(std::span<const float> samples);
    std::size_t push(std::span<const std::int16_t> samples);

    // Hands the stream to a new reader and returns its token. The reader of an older token reads silence from then on.
    [[nodiscard]] std::uint64_t claim() noexcept;

    // Reads interleaved float frames for the reader of the token on the mixing thread, which fills the frames that have not arrived with silence. A read that comes up short counts one underrun once samples started to arrive.
    void read(std::uint64_t token, std::span<float> output) noexcept;

    // Copies the newest samples that native code pushed, interleaved and in whole frames, oldest first, and returns how many samples it copied, which is fewer while less has arrived.
    std::size_t copyLatest(std::span<float> destination) const;

    // The frames that wait in the ring for the voice.
    [[nodiscard]] std::size_t getBufferedFrames() const noexcept;
    [[nodiscard]] std::uint64_t getUnderrunCount() const noexcept {
        return underruns.load(std::memory_order_relaxed);
    }

  private:
    // Converts the frames into the ring and the history, as floats.
    template <typename Sample> std::size_t write(std::span<const Sample> samples);

    const std::uint32_t sampleRate;
    const std::uint32_t channels;
    const Format format;
    const std::size_t capacity;

    // The ring holds capacity frames. The frame counters only grow, the producer advances written and the reader advances consumed, and each reads the other with acquire order.
    std::vector<float> ring;
    std::atomic<std::size_t> written{0};
    std::atomic<std::size_t> consumed{0};
    std::atomic<bool> started{false};
    std::atomic<std::uint64_t> reader{0};
    std::atomic<std::uint64_t> underruns{0};

    // The newest capacity frames that native code pushed, which the frame thread copies for analysis.
    mutable std::mutex historyMutex;
    std::vector<float> history;
    std::size_t historyFrames = 0;
};

} // namespace haylen::platform
