#pragma once

#include <cstdint>
#include <string>

#include "content/crypto/Digest.hpp"

namespace haylen::content {

// How a manifest names one immutable shard: its ID, the size of its file and the BLAKE2b-256 digest of the whole file, which a download checks before it keeps the file.
class ShardReference final {
  public:
    Digest shardId;
    std::uint64_t fileSize = 0;
    Digest fileDigest;

    // The file of a shard is named after its ID alone, so no name tells what the shard holds.
    [[nodiscard]] std::string getFileName() const;

    friend bool operator==(const ShardReference&, const ShardReference&) = default;
};

} // namespace haylen::content
