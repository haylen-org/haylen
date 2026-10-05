#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace haylen::content {

// Compresses each chunk of content on its own, so any chunk decodes without the others. A codec and its profile, which pins every setting that shapes the output, name how a chunk was stored, and both are part of its stored ID.
class Compression final {
  public:
    enum class Codec : std::uint8_t {
        None = 0,
        Zstd = 1,
    };

    static constexpr std::uint8_t kNoneProfile = 0;

    // Zstandard at level 12 in one frame that records its content size, without a checksum, which the encryption makes redundant, and without a dictionary.
    static constexpr std::uint8_t kZstdProfile = 1;

    struct Encoded {
        Codec codec = Codec::None;
        std::uint8_t profile = kNoneProfile;
        std::vector<std::uint8_t> bytes;
    };

    // Compresses a chunk, or stores it as it is when compression saves less than a thirty-second of it, as with media that is compressed already.
    [[nodiscard]] static Encoded encode(std::span<const std::uint8_t> plain);

    [[nodiscard]] static bool isSupported(std::uint8_t codec, std::uint8_t profile) noexcept;

    // Names the encoder that `encode` uses, its profiles and the version of its library, which together decide every encoded byte.
    [[nodiscard]] static std::string getEncoderName();

    // Decodes into a target of exactly the plain size, and throws `CorruptChunk` when the encoded bytes are not exactly that.
    static void decode(Codec codec, std::span<const std::uint8_t> encoded, std::span<std::uint8_t> plain);

  private:
    static constexpr int kZstdLevel = 12;
    static constexpr int kMaximumWindowLog = 23;
    static constexpr std::size_t kMinimumSavingDivisor = 32;
};

} // namespace haylen::content
