#pragma once

#include <miniaudio.h>

#include <cstdint>
#include <span>

namespace haylen::audio {

// Owns a decoder over encoded bytes that outputs 32-bit floats in the file's own channel count and sample rate.
class MemoryDecoder final {
  public:
    explicit MemoryDecoder(std::span<const std::uint8_t> encoded);
    ~MemoryDecoder();

    MemoryDecoder(const MemoryDecoder&) = delete;
    MemoryDecoder& operator=(const MemoryDecoder&) = delete;

    [[nodiscard]] ma_decoder& get() noexcept {
        return decoder;
    }

  private:
    ma_decoder decoder{};
};

} // namespace haylen::audio
