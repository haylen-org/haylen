#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include "content/format/Compression.hpp"

namespace haylen::content {

// Compresses the chunks of content for the codecs of `Compression`, on the machines that build releases.
class Compressor final {
  public:
    struct Encoded {
        Compression::Codec codec = Compression::Codec::None;
        std::uint8_t profile = Compression::kNoneProfile;
        std::vector<std::uint8_t> bytes;
    };

    // Compresses a chunk, or stores it as it is when compression saves less than a thirty-second of it, as with media that is compressed already.
    [[nodiscard]] static Encoded encode(std::span<const std::uint8_t> plain);

    // Names the encoder that `encode` uses, its profiles and the version of its library, which together decide every encoded byte.
    [[nodiscard]] static std::string getName();

  private:
    static constexpr int kZstdLevel = 12;
    static constexpr std::size_t kMinimumSavingDivisor = 32;
};

} // namespace haylen::content
