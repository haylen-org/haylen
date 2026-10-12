#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <vector>

#include "content/ShardReader.hpp"
#include "content/crypto/KeyRing.hpp"
#include "content/format/Catalog.hpp"
#include "content/format/ShardReference.hpp"
#include "haylen/io/PackageReader.hpp"

namespace haylen::content {

// The shards of one manifest, each opened the first time a chunk of it is read, so mounting a release of any size costs nothing until it is used. Only the shards used last stay open, and the one used least recently closes when another opens past that bound, so a release of any number of shards holds a bounded number of files and indexes. Several threads may read chunks at once, a shard opens once however many of them ask for it, and a shard that closes stays readable for the reads that already hold it.
class ShardSet final {
  public:
    static constexpr std::size_t kMaxOpenShards = 16;

    // Opens the file of a shard, or returns nothing when the file is missing.
    using Opener = std::function<std::unique_ptr<io::PackageReader>(const ShardReference&)>;

    ShardSet(std::vector<ShardReference> shardReferences, std::shared_ptr<const KeyRing> contentKeys, Opener shardOpener);

    // Reads a chunk of the catalog from its shard into the plain buffer, through the scratch buffer. Throws `MissingShard` when the file of its shard is missing.
    void readChunk(const Catalog::Chunk& chunk, std::vector<std::uint8_t>& scratch, std::vector<std::uint8_t>& plain) const;

  private:
    // The threads that open shards with the same stripe open them one at a time, so a shard opens once without a lock for each shard.
    static constexpr std::size_t kOpeningStripes = 16;

    [[nodiscard]] std::shared_ptr<const ShardReader> open(std::uint32_t shard) const;

    // Returns the open reader of the shard and marks it as used last, or null when the shard is closed.
    [[nodiscard]] std::shared_ptr<const ShardReader> findOpen(std::uint32_t shard) const;

    std::vector<ShardReference> references;
    std::shared_ptr<const KeyRing> keys;
    Opener opener;
    mutable std::array<std::mutex, kOpeningStripes> opening;
    mutable std::mutex mutex;
    mutable std::vector<std::shared_ptr<const ShardReader>> readers;
    mutable std::vector<std::uint32_t> recent;
};

} // namespace haylen::content
