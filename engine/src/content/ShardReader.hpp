#pragma once

#include <cstdint>
#include <memory>
#include <vector>

#include "content/crypto/ContentKey.hpp"
#include "content/crypto/KeyRing.hpp"
#include "content/format/ShardHeader.hpp"
#include "content/format/ShardIndex.hpp"
#include "content/format/ShardReference.hpp"
#include "haylen/io/PackageReader.hpp"

namespace haylen::content {

// Reads the chunks of one HPAK shard through a reader of its file, which it never loads whole. Opening checks the header against the reference of the manifest and decrypts the index into memory bounded by its entry limit. Several threads may read chunks at once.
class ShardReader final {
  public:
    ShardReader(std::unique_ptr<io::PackageReader> shardFile, const ShardReference& reference, const KeyRing& keys);

    // Reads one record, checks that it is the chunk the catalog names, then authenticates, decrypts, decodes and verifies it into the plain buffer. The scratch buffer holds the record on the way, so callers reuse both buffers across chunks.
    void readChunk(const Digest& storedId, const Digest& contentId, std::vector<std::uint8_t>& scratch, std::vector<std::uint8_t>& plain) const;

    [[nodiscard]] const ShardHeader& getHeader() const noexcept {
        return header;
    }
    [[nodiscard]] const ShardIndex& getIndex() const noexcept {
        return index;
    }

  private:
    [[nodiscard]] static ShardHeader readHeader(io::PackageReader& source, const ShardReference& reference);
    [[nodiscard]] static ShardIndex readIndex(io::PackageReader& source, const ShardHeader& parsed, const ContentKey& indexKey);

    std::unique_ptr<io::PackageReader> file;
    ShardHeader header;
    std::shared_ptr<const ContentKey> key;
    ShardIndex index;
};

} // namespace haylen::content
