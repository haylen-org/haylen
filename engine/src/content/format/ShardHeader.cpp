#include "content/format/ShardHeader.hpp"

#include <algorithm>

#include "content/Error.hpp"
#include "content/crypto/Hasher.hpp"
#include "content/format/BinaryReader.hpp"
#include "content/format/BinaryWriter.hpp"
#include "content/format/ShardIndex.hpp"

namespace haylen::content {

ShardHeader ShardHeader::parse(std::span<const std::uint8_t, kSize> bytes, std::uint64_t fileSize) {
    BinaryReader reader(bytes, Error::Code::CorruptHeader, "shard header");
    if (!std::ranges::equal(reader.readBytes(kMagic.size()), kMagic)) {
        throw Error(Error::Code::UnsupportedFormat, "The file is not an HPAK shard.");
    }
    if (reader.read<std::uint16_t>() != kVersion) {
        throw Error(Error::Code::UnsupportedVersion, "The HPAK shard has a version this app does not read.");
    }
    if (reader.read<std::uint16_t>() != kSize) {
        reader.fail("declares a size other than its version has");
    }
    if (reader.read<std::uint32_t>() != 0) {
        reader.fail("sets flags its version does not define");
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
        reader.fail("declares an index size that does not match its entry count");
    }
    if (header.indexOffset < kSize || header.indexOffset > fileSize || fileSize - header.indexOffset != header.indexSize) {
        reader.fail("places its index anywhere but at the end of the file");
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

void ShardHeader::sealIndex(const ContentKey& key, std::span<std::uint8_t> index) {
    const Bytes identity = serialize();
    indexNonce = key.deriveNonce(ContentKey::Purpose::ShardIndex, Hasher().update(std::span(identity).first(kNonceOffset)).update(index).finish().getBytes());
    const Bytes additional = serialize();
    indexTag = Aead::seal(key.getSubkey(ContentKey::Purpose::ShardIndex), indexNonce, std::span(additional).first(kTagOffset), index, index);
}

void ShardHeader::openIndex(const ContentKey& key, std::span<std::uint8_t> index) const {
    const Bytes additional = serialize();
    if (!Aead::open(key.getSubkey(ContentKey::Purpose::ShardIndex), indexNonce, std::span(additional).first(kTagOffset), indexTag, index, index)) {
        throw Error(Error::Code::CorruptIndex, "The index of the shard \"" + shardId.toHex() + "\" failed authentication, so the shard is damaged or was changed.");
    }
}

} // namespace haylen::content
