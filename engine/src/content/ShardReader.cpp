#include "content/ShardReader.hpp"

#include <monocypher.h>

#include <format>
#include <utility>

#include "content/format/ChunkRecord.hpp"
#include "haylen/content/Error.hpp"

namespace haylen::content {

ShardReader::ShardReader(std::unique_ptr<io::PackageReader> shardFile, const ShardReference& reference, const KeyRing& keys) : file(std::move(shardFile)), header(readHeader(*file, reference)), key(keys.get(header.keyId)), index(readIndex(*file, header, *key)) {}

ShardHeader ShardReader::readHeader(io::PackageReader& source, const ShardReference& reference) {
    const std::uint64_t size = source.getSize();
    if (size != reference.fileSize) {
        throw Error(Error::Code::CorruptHeader, std::format("The shard \"{}\" has {} bytes instead of the {} its manifest names.", reference.shardId.toHex(), size, reference.fileSize));
    }
    if (size < ShardHeader::kSize) {
        throw Error(Error::Code::CorruptHeader, std::format("The shard \"{}\" has {} bytes, fewer than its header takes.", reference.shardId.toHex(), size));
    }

    ShardHeader::Bytes bytes{};
    source.readExactly(0, bytes);
    ShardHeader parsed = ShardHeader::parse(bytes, size);
    if (parsed.shardId != reference.shardId) {
        throw Error(Error::Code::CorruptHeader, "The file of the shard \"" + reference.shardId.toHex() + "\" holds another shard.");
    }
    return parsed;
}

ShardIndex ShardReader::readIndex(io::PackageReader& source, const ShardHeader& parsed, const ContentKey& indexKey) {
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(parsed.indexSize));
    source.readExactly(parsed.indexOffset, bytes);
    parsed.openIndex(indexKey, bytes);
    ShardIndex entries = ShardIndex::parse(bytes, ShardHeader::kSize, parsed.indexOffset);
    crypto_wipe(bytes.data(), bytes.size());
    return entries;
}

void ShardReader::readChunk(const Catalog::Chunk& chunk, std::vector<std::uint8_t>& scratch, std::vector<std::uint8_t>& plain) const {
    const ShardIndex::Entry* entry = index.find(chunk.storedId);
    if (entry == nullptr) {
        throw Error(Error::Code::MissingChunk, "The shard \"" + header.shardId.toHex() + "\" does not hold the chunk \"" + chunk.storedId.toHex() + "\".");
    }

    scratch.resize(static_cast<std::size_t>(ChunkRecord::kHeaderSize + entry->encodedSize));
    file->readExactly(entry->recordOffset, scratch);
    const ChunkRecord record = ChunkRecord::parse(std::span(scratch).first<ChunkRecord::kHeaderSize>());

    // The record must be the chunk that the signed catalog names, with the sizes and the codec the catalog gives it, so a shard that someone rewrote with the content key still cannot change what an app reads, and a reader never trusts a size the record does not have.
    const bool matchesCatalog = record.storedId == chunk.storedId && record.contentId == chunk.contentId && record.codec == chunk.codec && record.profile == chunk.profile && record.plainSize == chunk.plainSize && record.encodedSize == chunk.encodedSize;
    const bool matchesIndex = record.codec == entry->codec && record.profile == entry->profile && record.plainSize == entry->plainSize && record.encodedSize == entry->encodedSize;
    if (!matchesCatalog || !matchesIndex) {
        throw Error(Error::Code::CorruptChunk, "The record of the chunk \"" + chunk.storedId.toHex() + "\" does not match the index of its shard and its catalog.");
    }
    record.open(*key, std::span(scratch).subspan(ChunkRecord::kHeaderSize), plain);
}

} // namespace haylen::content
