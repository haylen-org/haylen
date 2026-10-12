#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include "content/crypto/Digest.hpp"

namespace haylen::content {

// The first bytes of a record whose download stopped, kept in the staging folder of a local content store so the download resumes where it stopped: the shard and the offset of the record, its size, the validator of the response that sent the bytes and the encrypted bytes themselves. It never holds plain content, and the record is verified whole once its last byte arrives.
class PartialRecord final {
  public:
    static constexpr std::uint16_t kVersion = 1;
    static constexpr std::size_t kMaximumValidatorLength = 256;

    Digest shardId;
    std::uint64_t recordOffset = 0;
    std::uint64_t recordSize = 0;
    std::string validator;
    std::vector<std::uint8_t> bytes;

    [[nodiscard]] std::vector<std::uint8_t> write() const;

    // Reads a partial record, and throws `CacheCorrupt` for one that is damaged or holds no more bytes than a record may.
    [[nodiscard]] static PartialRecord parse(std::span<const std::uint8_t> file);

  private:
    static constexpr std::array<std::uint8_t, 8> kMagic = {'H', 'P', 'R', 'T', 0x0D, 0x0A, 0x1A, 0x0A};
};

} // namespace haylen::content
