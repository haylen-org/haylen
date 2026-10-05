#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

#include "content/crypto/Digest.hpp"
#include "content/format/Compression.hpp"

namespace haylen::content {

// The index of an HPAK shard: one entry of 64 bytes per stored chunk, sorted by stored ID, with where its record starts and how it is stored. It holds no paths, and it is encrypted at the end of the shard.
class ShardIndex final {
  public:
    static constexpr std::size_t kEntrySize = 64;

    // A shard holds at most this many chunks, which bounds the memory its index takes.
    static constexpr std::uint64_t kMaximumEntries = std::uint64_t{1} << 21;

    struct Entry {
        Digest storedId;
        std::uint64_t recordOffset = 0;
        std::uint64_t encodedSize = 0;
        std::uint64_t plainSize = 0;
        Compression::Codec codec = Compression::Codec::None;
        std::uint8_t profile = Compression::kNoneProfile;
    };

    // Sorts the entries by stored ID. Throws `std::invalid_argument` for two entries of one chunk.
    explicit ShardIndex(std::vector<Entry> chunks);

    // Reads a plain index and checks every entry: sorted unique IDs, sizes a chunk can have, and records that lie between the start and the end of the data and never overlap. Throws `CorruptIndex` or `InvalidOffset`.
    [[nodiscard]] static ShardIndex parse(std::span<const std::uint8_t> bytes, std::uint64_t dataBegin, std::uint64_t dataEnd);

    [[nodiscard]] std::vector<std::uint8_t> serialize() const;

    [[nodiscard]] const Entry* find(const Digest& storedId) const noexcept;
    [[nodiscard]] const std::vector<Entry>& getEntries() const noexcept {
        return entries;
    }

  private:
    static constexpr std::size_t kPaddingSize = 6;

    ShardIndex() = default;

    std::vector<Entry> entries;
};

} // namespace haylen::content
