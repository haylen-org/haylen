#include "content/format/StoreState.hpp"

#include <algorithm>

#include "content/format/BinaryReader.hpp"
#include "content/format/BinaryWriter.hpp"
#include "haylen/content/Error.hpp"

namespace haylen::content {

std::vector<std::uint8_t> StoreState::write() const {
    BinaryWriter writer;
    writer.writeBytes(kMagic).write(kVersion).writeZeros(6).write(sequence).writeDigest(application).write(appBuild).writeDigest(active).writeDigest(previous).writeDigest(pending);
    writer.write(acceptedGeneration).writeDigest(acceptedManifest).write(journal).writeString(channel);
    std::vector<std::uint8_t> bytes = writer.take();
    const Digest check = Digest::of(bytes);
    bytes.insert(bytes.end(), check.getBytes().begin(), check.getBytes().end());
    return bytes;
}

StoreState StoreState::parse(std::span<const std::uint8_t> bytes) {
    BinaryReader reader(bytes, Error::Code::CacheCorrupt, "store state");
    if (bytes.size() < Digest::kSize || bytes.size() > kMaximumSize) {
        reader.fail("has a size that is not the size of a state");
    }
    const std::span<const std::uint8_t> body = bytes.first(bytes.size() - Digest::kSize);
    if (Digest::of(body) != Digest(bytes.last<Digest::kSize>())) {
        reader.fail("has a checksum that does not match, so a write tore it");
    }

    BinaryReader fields(body, Error::Code::CacheCorrupt, "store state");
    const std::span<const std::uint8_t> magic = fields.readBytes(kMagic.size());
    if (!std::equal(magic.begin(), magic.end(), kMagic.begin())) {
        fields.fail("does not start with the magic of a state");
    }
    if (fields.read<std::uint16_t>() != kVersion) {
        fields.fail("has a version this build does not read");
    }
    fields.skipZeros(6);

    StoreState state;
    state.sequence = fields.read<std::uint64_t>();
    state.application = fields.readDigest();
    state.appBuild = fields.read<std::uint64_t>();
    state.active = fields.readDigest();
    state.previous = fields.readDigest();
    state.pending = fields.readDigest();
    state.acceptedGeneration = fields.read<std::uint64_t>();
    state.acceptedManifest = fields.readDigest();
    state.journal = fields.read<std::uint64_t>();
    state.channel = std::string(fields.readString(kMaximumChannelLength));
    fields.requireEnd();
    return state;
}

} // namespace haylen::content
