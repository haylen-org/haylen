#include "content/format/ShardHeader.hpp"

#include <algorithm>

#include "content/format/BinaryReader.hpp"
#include "content/format/BinaryWriter.hpp"
#include "content/format/ShardIndex.hpp"
#include "haylen/content/Error.hpp"

namespace haylen::content {

ShardHeader ShardHeader::parse(std::span<const std::uint8_t, kSize> bytes, std::uint64_t fileSize) {
    BinaryReader reader(bytes, Error::Code::CorruptHeader, "The shard");
    if (!std::ranges::equal(reader.readBytes(kMagic.size()), kMagic)) {
        throw Error(Error::Code::UnsupportedFormat, "The file is not a shard of a Haylen release.");
    }

    if (reader.read<std::uint16_t>() != kVersion) {
        throw Error(Error::Code::UnsupportedVersion, "The shard was made by another version of Haylen. Build the release again with this version.");
    }

    if (reader.read<std::uint16_t>() != kSize) {
        reader.fail();
    }

    if (reader.read<std::uint32_t>() != 0) {
        reader.fail();
    }

    ShardHeader header;
    header.shardId = reader.readDigest();
    header.keyId = reader.readDigest();
    header.indexOffset = reader.read<std::uint64_t>();
    header.indexSize = reader.read<std::uint64_t>();
    header.entryCount = reader.read<std::uint64_t>();
    std::ranges::copy(reader.readBytes(Aead::kNonceSize), header.indexNonce.begin());
    reader.skipZeros(kReservedSize);
    std::ranges::copy(reader.readBytes(Aead::kTagSize), header.indexTag.begin());

    // The counts are checked before they are multiplied, so no product wraps around.
    if (header.entryCount == 0 || header.entryCount > ShardIndex::kMaximumEntries || header.indexSize != header.entryCount * ShardIndex::kEntrySize) {
        reader.fail();
    }

    if (header.indexOffset < kSize || header.indexOffset > fileSize || fileSize - header.indexOffset != header.indexSize) {
        reader.fail();
    }

    return header;
}

ShardHeader::Bytes ShardHeader::serialize() const {
    BinaryWriter writer;
    writer.writeBytes(kMagic).write(kVersion).write(static_cast<std::uint16_t>(kSize)).write(std::uint32_t{0});
    writer.writeDigest(shardId).writeDigest(keyId).write(indexOffset).write(indexSize).write(entryCount);
    writer.writeBytes(indexNonce).writeZeros(kReservedSize).writeBytes(indexTag);

    Bytes bytes{};
    std::ranges::copy(writer.getBytes(), bytes.begin());
    return bytes;
}

void ShardHeader::openIndex(const ContentKey& key, std::span<std::uint8_t> index) const {
    const Bytes additional = serialize();
    if (!Aead::open(key.getSubkey(ContentKey::Purpose::ShardIndex), indexNonce, std::span(additional).first(kTagOffset), indexTag, index, index)) {
        throw Error(Error::Code::CorruptIndex, "The index of the shard \"" + shardId.toHex() + "\" failed authentication, so the shard is damaged or was changed.");
    }
}

} // namespace haylen::content
