#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

#include "content/crypto/Digest.hpp"

namespace haylen::content {

// One entry of the journal of a local content store: where a record lies, by its stored ID, with the ID of the content key that sealed it, or the removal of a record. Entries have a fixed size and a checksum each, so the journal only grows, and an entry that a crash tore is found and dropped.
class StoreEntry final {
  public:
    static constexpr std::size_t kSize = 96;

    using Bytes = std::array<std::uint8_t, kSize>;

    Digest storedId;
    Digest keyId;
    std::uint32_t segment = 0;
    std::uint64_t offset = 0;
    std::uint64_t size = 0;
    bool removed = false;

    [[nodiscard]] Bytes write() const;

    // Reads an entry, or returns nothing for one whose checksum or fields do not hold.
    [[nodiscard]] static std::optional<StoreEntry> parse(std::span<const std::uint8_t, kSize> bytes);

  private:
    static constexpr std::size_t kCheckOffset = 88;
    static constexpr std::size_t kCheckSize = 8;
    static constexpr std::uint32_t kRemovedFlag = 1;
};

} // namespace haylen::content
