#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <span>
#include <vector>

#include "content/crypto/ContentKey.hpp"
#include "content/crypto/Digest.hpp"
#include "content/format/ChunkRecord.hpp"

namespace haylen::content {

// The build cache of sealed records in a folder, found by the content key that sealed them and the content ID of their chunk, which saves compressing and encrypting unchanged chunks again. Records hold only ciphertext, so the cache holds no plain content, and sealing is deterministic, so a cached record is the record a fresh build writes. A record serves only when it authenticates under the key and decodes to the chunk asked for, so a damaged or foreign entry only costs the time to seal its chunk again, and deleting the folder never changes a build.
class RecordCache final {
  public:
    explicit RecordCache(std::filesystem::path cacheFolder);

    // Returns the record of a chunk with its ciphertext, when the cache holds one that verifies.
    [[nodiscard]] std::optional<ChunkRecord> find(const ContentKey& key, const Digest& contentId, std::vector<std::uint8_t>& ciphertext) const;
    void store(const ContentKey& key, const ChunkRecord& record, std::span<const std::uint8_t> ciphertext) const;

  private:
    [[nodiscard]] std::filesystem::path locate(const ContentKey& key, const Digest& contentId) const;

    std::filesystem::path folder;
};

} // namespace haylen::content
