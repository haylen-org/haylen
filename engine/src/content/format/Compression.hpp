#pragma once

#include <cstddef>
#include <cstdint>
#include <span>

namespace haylen::content {

// The codecs of the chunks of content, each compressed on its own, so any chunk decodes without the others. A codec and its profile, which pins every setting that shapes the output, name how a chunk was stored, and both are part of its stored ID. The runtime only decodes, and `Compressor` of the content tool encodes.
class Compression final {
  public:
    enum class Codec : std::uint8_t {
        None = 0,
        Zstd = 1,
    };

    static constexpr std::uint8_t kNoneProfile = 0;

    // Zstandard at level 12 in one frame that records its content size, without a checksum, which the encryption makes redundant, and without a dictionary.
    static constexpr std::uint8_t kZstdProfile = 1;

    [[nodiscard]] static bool isSupported(std::uint8_t codec, std::uint8_t profile) noexcept;

    // Decodes into a target of exactly the plain size, and throws `CorruptChunk` when the encoded bytes are not exactly that.
    static void decode(Codec codec, std::span<const std::uint8_t> encoded, std::span<std::uint8_t> plain);

  private:
    static constexpr int kMaximumWindowLog = 23;
};

} // namespace haylen::content
