#pragma once

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

// The shards of one manifest, each opened the first time a chunk of it is read, so mounting a release of any size costs nothing until it is used. Several threads may read chunks at once, and a shard opens once however many of them ask for it.
class ShardSet final {
  public:
    // Opens the file of a shard, or returns nothing when the file is missing.
    using Opener = std::function<std::unique_ptr<io::PackageReader>(const ShardReference&)>;

    ShardSet(std::vector<ShardReference> shardReferences, std::shared_ptr<const KeyRing> contentKeys, Opener shardOpener);

    // Reads a chunk of the catalog from its shard into the plain buffer, through the scratch buffer. Throws `MissingShard` when the file of its shard is missing.
    void readChunk(const Catalog::Chunk& chunk, std::vector<std::uint8_t>& scratch, std::vector<std::uint8_t>& plain) const;

  private:
    struct Slot {
        std::mutex mutex;
        std::unique_ptr<const ShardReader> reader;
    };

    [[nodiscard]] const ShardReader& open(std::uint32_t shard) const;

    std::vector<ShardReference> references;
    std::shared_ptr<const KeyRing> keys;
    Opener opener;
    mutable std::vector<Slot> slots;
};

} // namespace haylen::content
