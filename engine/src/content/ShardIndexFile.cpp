#include "content/ShardIndexFile.hpp"

#include <algorithm>
#include <format>
#include <utility>

#include "haylen/content/Error.hpp"

namespace haylen::content {

ShardIndexFile::ShardIndexFile(std::vector<std::uint8_t> headerAndIndex, const ShardReference& reference) : bytes(std::move(headerAndIndex)), fileSize(reference.fileSize), indexOffset(0) {
    if (bytes.size() < ShardHeader::kSize) {
        throw Error(Error::Code::CorruptHeader, std::format("The header of the shard \"{}\" is incomplete.", reference.shardId.toHex()));
    }
    const ShardHeader header = ShardHeader::parse(std::span(bytes).first<ShardHeader::kSize>(), fileSize);
    if (bytes.size() != ShardHeader::kSize + header.indexSize) {
        throw Error(Error::Code::CorruptHeader, std::format("The index of the shard \"{}\" does not have the size its header names.", reference.shardId.toHex()));
    }
    indexOffset = header.indexOffset;
}

std::size_t ShardIndexFile::read(std::uint64_t offset, std::span<std::uint8_t> target) {
    // Only the header at the start and the index at the end exist, and a read must lie inside one of them.
    const std::uint64_t indexSize = bytes.size() - ShardHeader::kSize;
    std::size_t source = 0;
    if (offset + target.size() <= ShardHeader::kSize) {
        source = static_cast<std::size_t>(offset);
    } else if (offset >= indexOffset && offset - indexOffset + target.size() <= indexSize) {
        source = static_cast<std::size_t>(ShardHeader::kSize + offset - indexOffset);
    } else {
        throw Error(Error::Code::MissingChunk, std::format("The bytes {} to {} of a shard are not on the device.", offset, offset + target.size()));
    }
    std::copy_n(bytes.begin() + static_cast<std::ptrdiff_t>(source), target.size(), target.begin());
    return target.size();
}

} // namespace haylen::content
