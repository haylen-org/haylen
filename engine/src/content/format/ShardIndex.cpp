#include "content/format/ShardIndex.hpp"

#include <algorithm>
#include <stdexcept>
#include <utility>

#include "content/Error.hpp"
#include "content/format/BinaryReader.hpp"
#include "content/format/BinaryWriter.hpp"
#include "content/format/ChunkRecord.hpp"

namespace haylen::content {

ShardIndex::ShardIndex(std::vector<Entry> chunks) : entries(std::move(chunks)) {
    std::ranges::sort(entries, {}, &Entry::storedId);
    if (std::ranges::adjacent_find(entries, {}, &Entry::storedId) != entries.end()) {
        throw std::invalid_argument("A shard index cannot hold one chunk twice.");
    }
}

ShardIndex ShardIndex::parse(std::span<const std::uint8_t> bytes, std::uint64_t dataBegin, std::uint64_t dataEnd) {
    BinaryReader reader(bytes, Error::Code::CorruptIndex, "shard index");
    if (bytes.size() % kEntrySize != 0 || bytes.size() / kEntrySize > kMaximumEntries) {
        reader.fail("has a size that is no whole number of entries within its limit");
    }

    ShardIndex index;
    index.entries.reserve(bytes.size() / kEntrySize);
    while (reader.getRemaining() > 0) {
        Entry entry;
        entry.storedId = reader.readDigest();
        entry.recordOffset = reader.read<std::uint64_t>();
        entry.encodedSize = reader.read<std::uint64_t>();
        entry.plainSize = reader.read<std::uint64_t>();
        const auto codec = reader.read<std::uint8_t>();
        entry.profile = reader.read<std::uint8_t>();
        reader.skipZeros(kPaddingSize);

        if (!Compression::isSupported(codec, entry.profile)) {
            reader.fail("names a codec this app does not decode");
        }
        entry.codec = static_cast<Compression::Codec>(codec);
        if (!ChunkRecord::hasValidSizes(entry.codec, entry.plainSize, entry.encodedSize)) {
            reader.fail("declares sizes no chunk has");
        }
        if (!index.entries.empty() && !(index.entries.back().storedId < entry.storedId)) {
            reader.fail("is not sorted by stored ID without repeats");
        }
        if (entry.recordOffset < dataBegin || entry.recordOffset > dataEnd || dataEnd - entry.recordOffset < ChunkRecord::kHeaderSize + entry.encodedSize) {
            throw Error(Error::Code::InvalidOffset, "The shard index places a record outside the data of its shard.");
        }
        index.entries.push_back(entry);
    }

    std::vector<std::pair<std::uint64_t, std::uint64_t>> records;
    records.reserve(index.entries.size());
    for (const Entry& entry : index.entries) {
        records.emplace_back(entry.recordOffset, entry.recordOffset + ChunkRecord::kHeaderSize + entry.encodedSize);
    }
    std::ranges::sort(records);
    for (std::size_t record = 1; record < records.size(); ++record) {
        if (records[record].first < records[record - 1].second) {
            throw Error(Error::Code::InvalidOffset, "The shard index places two records over each other.");
        }
    }
    return index;
}

std::vector<std::uint8_t> ShardIndex::serialize() const {
    BinaryWriter writer;
    for (const Entry& entry : entries) {
        writer.writeDigest(entry.storedId).write(entry.recordOffset).write(entry.encodedSize).write(entry.plainSize);
        writer.write(static_cast<std::uint8_t>(entry.codec)).write(entry.profile).writeZeros(kPaddingSize);
    }
    return writer.take();
}

const ShardIndex::Entry* ShardIndex::find(const Digest& storedId) const noexcept {
    const auto found = std::ranges::lower_bound(entries, storedId, {}, &Entry::storedId);
    return found != entries.end() && found->storedId == storedId ? &*found : nullptr;
}

} // namespace haylen::content
