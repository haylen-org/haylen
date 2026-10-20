#include "content/format/StoreEntry.hpp"

#include <algorithm>
#include <limits>

#include "content/crypto/Hasher.hpp"
#include "content/format/BinaryReader.hpp"
#include "content/format/BinaryWriter.hpp"
#include "content/format/ChunkRecord.hpp"
#include "haylen/content/Error.hpp"

namespace haylen::content {

StoreEntry::Bytes StoreEntry::write() const {
    BinaryWriter writer;
    writer.writeDigest(storedId).writeDigest(keyId).write(segment).write(removed ? kRemovedFlag : std::uint32_t{0}).write(offset).write(size);
    Bytes bytes{};
    std::ranges::copy(writer.getBytes(), bytes.begin());
    Hasher(kCheckSize).update(std::span(bytes).first<kCheckOffset>()).finish(std::span(bytes).subspan<kCheckOffset>());
    return bytes;
}

std::optional<StoreEntry> StoreEntry::parse(std::span<const std::uint8_t, kSize> bytes) {
    std::array<std::uint8_t, kCheckSize> check{};
    Hasher(kCheckSize).update(bytes.first<kCheckOffset>()).finish(check);
    if (!std::ranges::equal(check, bytes.subspan<kCheckOffset>())) {
        return std::nullopt;
    }

    BinaryReader reader(bytes.first<kCheckOffset>(), Error::Code::CacheCorrupt, "store entry");
    StoreEntry entry{.storedId = reader.readDigest(), .keyId = reader.readDigest(), .segment = reader.read<std::uint32_t>()};
    const auto flags = reader.read<std::uint32_t>();
    entry.offset = reader.read<std::uint64_t>();
    entry.size = reader.read<std::uint64_t>();
    entry.removed = flags == kRemovedFlag;

    // A record is at least a header and at most a header with the largest chunk, and a removal names no place.
    const bool validRecord = entry.size > ChunkRecord::kHeaderSize && entry.size <= ChunkRecord::kHeaderSize + ChunkRecord::kMaximumPlainSize && entry.offset <= std::numeric_limits<std::uint64_t>::max() - entry.size;
    if ((flags != 0 && flags != kRemovedFlag) || (!entry.removed && !validRecord)) {
        return std::nullopt;
    }
    return entry;
}

} // namespace haylen::content
