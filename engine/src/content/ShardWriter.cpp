#include "content/ShardWriter.hpp"

#include <array>
#include <stdexcept>
#include <system_error>
#include <utility>

#include "content/crypto/Hasher.hpp"
#include "content/format/ShardHeader.hpp"

namespace haylen::content {

ShardWriter::ShardWriter(const std::filesystem::path& outputFolder, std::shared_ptr<const ContentKey> contentKey) : folder(outputFolder), temporary(outputFolder / "shard.hpak.partial"), key(std::move(contentKey)) {
    std::filesystem::create_directories(folder);
    stream.open(temporary, std::ios::binary | std::ios::trunc);
    if (!stream) {
        throw std::runtime_error("The shard file \"" + temporary.generic_string() + "\" could not be created.");
    }

    // The header is written last, when the index it locates exists, so its place is reserved first.
    const ShardHeader::Bytes placeholder{};
    write(placeholder);
}

ShardWriter::~ShardWriter() {
    if (!finished) {
        stream.close();
        std::error_code error;
        std::filesystem::remove(temporary, error);
    }
}

void ShardWriter::write(std::span<const std::uint8_t> bytes) {
    stream.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    if (!stream) {
        throw std::runtime_error("The shard file \"" + temporary.generic_string() + "\" could not be written.");
    }
    offset += bytes.size();
}

void ShardWriter::add(const ChunkRecord& record, std::span<const std::uint8_t> ciphertext) {
    if (!stored.insert(record.storedId).second) {
        return;
    }
    entries.push_back({.storedId = record.storedId, .recordOffset = offset, .encodedSize = record.encodedSize, .plainSize = record.plainSize, .codec = record.codec, .profile = record.profile});
    write(record.serialize());
    write(ciphertext);
}

Digest ShardWriter::identify() const {
    // The ID covers the key and every record in the order of the file, so two shards share an ID only when they hold the same bytes.
    Hasher hasher;
    hasher.updateLabel("haylen/hpak/v1/shard-id").update(key->getId());
    std::array<std::uint8_t, sizeof(std::uint64_t)> position{};
    for (const ShardIndex::Entry& entry : entries) {
        for (std::size_t index = 0; index < position.size(); ++index) {
            position[index] = static_cast<std::uint8_t>(entry.recordOffset >> (8 * index));
        }
        hasher.update(entry.storedId).update(position);
    }
    return hasher.finish();
}

ShardReference ShardWriter::finish() {
    ShardHeader header;
    header.shardId = identify();
    header.keyId = key->getId();
    header.indexOffset = offset;
    header.entryCount = entries.size();

    std::vector<std::uint8_t> index = ShardIndex(entries).serialize();
    header.indexSize = index.size();
    header.sealIndex(*key, index);
    write(index);

    const ShardHeader::Bytes bytes = header.serialize();
    stream.seekp(0);
    stream.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    stream.close();
    if (!stream) {
        throw std::runtime_error("The shard file \"" + temporary.generic_string() + "\" could not be completed.");
    }

    const ShardReference reference{.shardId = header.shardId, .fileSize = header.indexOffset + header.indexSize, .fileDigest = digestFile()};
    std::filesystem::rename(temporary, folder / reference.getFileName());
    finished = true;
    return reference;
}

Digest ShardWriter::digestFile() const {
    std::ifstream file(temporary, std::ios::binary);
    std::vector<std::uint8_t> buffer(kDigestBufferSize);
    Hasher hasher;
    while (file) {
        file.read(reinterpret_cast<char*>(buffer.data()), static_cast<std::streamsize>(buffer.size()));
        hasher.update(std::span(buffer).first(static_cast<std::size_t>(file.gcount())));
    }
    return hasher.finish();
}

} // namespace haylen::content
