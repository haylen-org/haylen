#pragma once

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <memory>
#include <span>
#include <unordered_set>
#include <vector>

#include "content/crypto/ContentKey.hpp"
#include "content/format/ChunkRecord.hpp"
#include "content/format/ShardIndex.hpp"
#include "content/format/ShardReference.hpp"

namespace haylen::content {

// Writes one immutable HPAK shard into a folder: the header, the sealed records in the order they come, and the encrypted index. The file grows under a temporary name and takes the name of its ID only when it is complete, and a writer that ends unfinished removes it.
class ShardWriter final {
  public:
    ShardWriter(const std::filesystem::path& outputFolder, std::shared_ptr<const ContentKey> contentKey);
    ~ShardWriter();

    ShardWriter(const ShardWriter&) = delete;
    ShardWriter& operator=(const ShardWriter&) = delete;

    // Appends a sealed record, unless the shard holds its chunk already.
    void add(const ChunkRecord& record, std::span<const std::uint8_t> ciphertext);

    [[nodiscard]] bool contains(const Digest& storedId) const noexcept {
        return stored.contains(storedId);
    }

    // The size the shard file has once its index is written.
    [[nodiscard]] std::uint64_t getSize() const noexcept {
        return offset + entries.size() * ShardIndex::kEntrySize;
    }
    [[nodiscard]] std::uint64_t getEntryCount() const noexcept {
        return entries.size();
    }

    [[nodiscard]] ShardReference finish();

  private:
    static constexpr std::size_t kDigestBufferSize = 1024 * 1024;

    [[nodiscard]] Digest identify() const;
    [[nodiscard]] Digest digestFile() const;
    void write(std::span<const std::uint8_t> bytes);

    std::filesystem::path folder;
    std::filesystem::path temporary;
    std::shared_ptr<const ContentKey> key;
    std::ofstream stream;
    std::uint64_t offset = 0;
    std::vector<ShardIndex::Entry> entries;
    std::unordered_set<Digest, Digest::Hash> stored;
    bool finished = false;
};

} // namespace haylen::content
