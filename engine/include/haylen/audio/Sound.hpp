#pragma once

#include <cstdint>
#include <memory>
#include <span>
#include <utility>
#include <vector>

namespace haylen::audio {

struct SoundData;

// Shared handle to audio. Decoded sounds keep 32-bit float frames in memory, which suits short effects. Streamed sounds keep the encoded file and decode it while they play, which suits long music.
class Sound final {
  public:
    Sound() = default;
    explicit Sound(std::shared_ptr<const SoundData> value) noexcept : data(std::move(value)) {}

    // Both factories accept WAV, FLAC, MP3 and Ogg Vorbis data and throw `std::runtime_error` when it cannot be decoded.
    [[nodiscard]] static Sound decode(std::span<const std::uint8_t> encoded);
    [[nodiscard]] static Sound stream(std::vector<std::uint8_t> encoded);

    // Makes a decoded sound from raw samples, interleaved by channel, where 16-bit samples become floats. Throws `std::invalid_argument` for a sample rate or channels of 0 and for samples that do not fill whole frames, and `std::runtime_error` for no samples at all.
    [[nodiscard]] static Sound fromSamples(std::span<const float> samples, std::uint32_t channels, std::uint32_t sampleRate);
    [[nodiscard]] static Sound fromSamples(std::span<const std::int16_t> samples, std::uint32_t channels, std::uint32_t sampleRate);

    [[nodiscard]] bool isValid() const noexcept {
        return data != nullptr;
    }
    [[nodiscard]] bool isStreamed() const noexcept;
    [[nodiscard]] std::uint32_t getChannels() const noexcept;
    [[nodiscard]] std::uint32_t getSampleRate() const noexcept;
    [[nodiscard]] std::uint64_t getFrameCount() const noexcept;
    [[nodiscard]] float getDuration() const noexcept;
    [[nodiscard]] const std::shared_ptr<const SoundData>& getData() const noexcept {
        return data;
    }
    [[nodiscard]] bool operator==(const Sound& other) const noexcept {
        return data == other.data;
    }

  private:
    std::shared_ptr<const SoundData> data;
};

} // namespace haylen::audio
