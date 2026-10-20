#include "content/format/PartialRecord.hpp"

#include <algorithm>

#include "content/format/BinaryReader.hpp"
#include "content/format/BinaryWriter.hpp"
#include "content/format/ChunkRecord.hpp"
#include "haylen/content/Error.hpp"

namespace haylen::content {

std::vector<std::uint8_t> PartialRecord::write() const {
    BinaryWriter writer;
    writer.writeBytes(kMagic).write(kVersion).writeZeros(6).writeDigest(shardId).write(recordOffset).write(recordSize).writeString(validator).writeBytes(bytes);
    return writer.take();
}

PartialRecord PartialRecord::parse(std::span<const std::uint8_t> file) {
    BinaryReader reader(file, Error::Code::CacheCorrupt, "partial record");
    const std::span<const std::uint8_t> magic = reader.readBytes(kMagic.size());
    if (!std::ranges::equal(magic, kMagic)) {
        reader.fail("does not start with the magic of a partial record");
    }
    if (reader.read<std::uint16_t>() != kVersion) {
        reader.fail("has a version this build does not read");
    }
    reader.skipZeros(6);

    PartialRecord partial{.shardId = reader.readDigest(), .recordOffset = reader.read<std::uint64_t>(), .recordSize = reader.read<std::uint64_t>()};
    partial.validator = std::string(reader.readString(kMaximumValidatorLength));
    const std::span<const std::uint8_t> held = reader.readBytes(reader.getRemaining());
    if (partial.recordSize <= ChunkRecord::kHeaderSize || partial.recordSize > ChunkRecord::kHeaderSize + ChunkRecord::kMaximumPlainSize || held.empty() || held.size() >= partial.recordSize || partial.validator.empty()) {
        reader.fail("has sizes that are not those of a record that stopped part way");
    }
    partial.bytes.assign(held.begin(), held.end());
    return partial;
}

} // namespace haylen::content
