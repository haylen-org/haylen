#pragma once

#include <cstdint>
#include <span>
#include <vector>

#include "content/crypto/ContentKey.hpp"
#include "content/crypto/Digest.hpp"
#include "content/format/ChunkRecord.hpp"

namespace haylen::content {

// Seals the chunks of content into the records of HPAK shards on the machines that build releases. The nonce derives from the header, so the same chunk always seals to the same bytes.
class RecordSealer final {
  public:
    // Encodes and encrypts a plain chunk whose content ID the caller computed already, and replaces the ciphertext with the bytes that follow the header.
    [[nodiscard]] static ChunkRecord seal(const ContentKey& key, std::span<const std::uint8_t> plain, const Digest& plainId, std::vector<std::uint8_t>& ciphertext);
};

} // namespace haylen::content
