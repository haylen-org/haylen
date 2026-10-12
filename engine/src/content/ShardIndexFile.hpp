#pragma once

#include <cstdint>
#include <span>
#include <vector>

#include "content/format/ShardHeader.hpp"
#include "content/format/ShardReference.hpp"
#include "haylen/io/PackageReader.hpp"

namespace haylen::content {

// The header and the encrypted index of a shard whose records come by download, as one file of the size of the shard that holds only those two parts, so `ShardReader` checks and decrypts them exactly as it does for a shard on the device. Reading any other byte throws, since the records arrive on their own. The bytes are the header followed by the index, which is how the local store keeps them.
class ShardIndexFile final : public io::PackageReader {
  public:
    // Takes the header followed by the encrypted index. Throws `CorruptHeader` when the header is not one of the shard or the index does not end it.
    ShardIndexFile(std::vector<std::uint8_t> headerAndIndex, const ShardReference& reference);

    [[nodiscard]] std::uint64_t getSize() const noexcept override {
        return fileSize;
    }
    [[nodiscard]] std::size_t read(std::uint64_t offset, std::span<std::uint8_t> target) override;

  private:
    std::vector<std::uint8_t> bytes;
    std::uint64_t fileSize;
    std::uint64_t indexOffset;
};

} // namespace haylen::content
