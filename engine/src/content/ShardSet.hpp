#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <vector>

#include "content/LocalStore.hpp"
#include "content/ShardReader.hpp"
#include "content/crypto/KeyRing.hpp"
#include "content/format/Catalog.hpp"
#include "content/format/ShardReference.hpp"
#include "haylen/io/PackageReader.hpp"

namespace haylen::content {

// The chunks of one manifest, read from the shard files that the device holds or else from the records of the local store that downloads brought. Each shard opens the first time a chunk of it is read, so mounting a release of any size costs nothing until it is used. Only the shards used last stay open, and the one used least recently closes when another opens past that bound, so a release of any number of shards holds a bounded number of files and indexes. Several threads may read chunks at once, a shard opens once however many of them ask for it, and a shard that closes stays readable for the reads that already hold it.
class ShardSet final {
  public:
    static constexpr std::size_t kMaxOpenShards = 16;

    // What the set read since it was made: the shards open now, the chunks it decoded with their plain bytes and the time it took to read, decrypt, decompress and check them, and the reads that run now.
    struct Reads {
        std::uint64_t openShards = 0;
        std::uint64_t decodedChunks = 0;
        std::uint64_t decodedBytes = 0;
        std::uint64_t decodeNanoseconds = 0;
        std::uint64_t activeReads = 0;
    };

    // Opens the file of a shard, or returns nothing when the device does not hold it.
    using Opener = std::function<std::unique_ptr<io::PackageReader>(const ShardReference&)>;

    ShardSet(std::vector<ShardReference> shardReferences, std::shared_ptr<const KeyRing> contentKeys, Opener shardOpener, std::shared_ptr<LocalStore> recordStore = nullptr);

    // Reads a chunk of the catalog into the plain buffer, through the scratch buffer. Throws `MissingShard` when the device lacks the file of its shard and the set has no store, `MissingChunk` when the store lacks its record too, and `CacheCorrupt` when its record in the store fails its checks, which removes it so a download replaces it.
    void readChunk(const Catalog::Chunk& chunk, std::vector<std::uint8_t>& scratch, std::vector<std::uint8_t>& plain) const;

    // Tells whether the device holds the chunk, in the file of its shard or in the store, so reading it never waits for a download.
    [[nodiscard]] bool hasChunk(const Catalog::Chunk& chunk) const;

    // Opens a shard whose file the device holds, or returns null. A missing file stays missing until `forgetMissing`.
    [[nodiscard]] std::shared_ptr<const ShardReader> findShard(std::uint32_t shard) const;

    // Looks again for the shard files that were missing, once a store delivered some of them.
    void forgetMissing() const;

    [[nodiscard]] Reads getReads() const;

    [[nodiscard]] const std::vector<ShardReference>& getReferences() const noexcept {
        return references;
    }
    [[nodiscard]] const std::shared_ptr<LocalStore>& getStore() const noexcept {
        return store;
    }
    [[nodiscard]] const KeyRing& getKeys() const noexcept {
        return *keys;
    }

  private:
    // The threads that open shards with the same stripe open them one at a time, so a shard opens once without a lock for each shard.
    static constexpr std::size_t kOpeningStripes = 16;

    // Returns the open reader of the shard and marks it as used last, or null when the shard is closed.
    [[nodiscard]] std::shared_ptr<const ShardReader> findOpen(std::uint32_t shard) const;
    void readRecord(const Catalog::Chunk& chunk, std::vector<std::uint8_t>& scratch, std::vector<std::uint8_t>& plain) const;

    std::vector<ShardReference> references;
    std::shared_ptr<const KeyRing> keys;
    Opener opener;
    std::shared_ptr<LocalStore> store;
    mutable std::array<std::mutex, kOpeningStripes> opening;
    mutable std::mutex mutex;
    mutable std::vector<std::shared_ptr<const ShardReader>> readers;
    mutable std::vector<bool> missing;
    mutable std::vector<std::uint32_t> recent;
    mutable std::atomic<std::uint64_t> decodedChunks = 0;
    mutable std::atomic<std::uint64_t> decodedBytes = 0;
    mutable std::atomic<std::uint64_t> decodeNanoseconds = 0;
    mutable std::atomic<std::uint64_t> activeReads = 0;
};

} // namespace haylen::content
