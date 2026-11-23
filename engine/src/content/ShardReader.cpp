#include "content/ShardReader.hpp"

#include <monocypher.h>

#include <format>
#include <optional>
#include <utility>

#include "content/format/ChunkRecord.hpp"
#include "haylen/content/Error.hpp"

namespace haylen::content {

ShardReader::ShardReader(std::unique_ptr<io::PackageReader> shardFile, const ShardReference& reference, const KeyRing& keys) : file(std::move(shardFile)), name(reference.getFileName()), header(readHeader(*file, reference)), key(keys.get(header.keyId)), index(readIndex(*file, header, *key, name)) {}

ShardHeader ShardReader::readHeader(io::PackageReader& source, const ShardReference& reference) {
    const std::uint64_t size = source.getSize();
    if (size != reference.fileSize) {
        throw Error(Error::Code::CorruptHeader, std::format("The shard \"{}\" is damaged or incomplete, since it has {} bytes and its manifest names {}.", reference.getFileName(), size, reference.fileSize));
    }

    if (size < ShardHeader::kSize) {
        throw Error(Error::Code::CorruptHeader, std::format("The shard \"{}\" is damaged or was not made by this version of Haylen.", reference.getFileName()));
    }

    ShardHeader::Bytes bytes{};
    source.readExactly(0, bytes);
    std::optional<ShardHeader> parsed;
    try {
        parsed = ShardHeader::parse(bytes, size);
    } catch (const Error& failure) {
        throw nameFile(failure, reference.getFileName());
    }

    if (parsed->shardId != reference.shardId) {
        throw Error(Error::Code::CorruptHeader, "The file \"" + reference.getFileName() + "\" holds another shard than its name says.");
    }

    return *parsed;
}

ShardIndex ShardReader::readIndex(io::PackageReader& source, const ShardHeader& parsed, const ContentKey& indexKey, const std::string& fileName) {
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(parsed.indexSize));
    source.readExactly(parsed.indexOffset, bytes);

    // The decrypted index leaves memory whether or not it parses.
    try {
        parsed.openIndex(indexKey, bytes);
        ShardIndex entries = ShardIndex::parse(bytes, ShardHeader::kSize, parsed.indexOffset);
        crypto_wipe(bytes.data(), bytes.size());
        return entries;
    } catch (const Error& failure) {
        crypto_wipe(bytes.data(), bytes.size());
        throw nameFile(failure, fileName);
    } catch (...) {
        crypto_wipe(bytes.data(), bytes.size());
        throw;
    }
}

Error ShardReader::nameFile(const Error& failure, const std::string& fileName) {
    return {failure.getCode(), "The shard \"" + fileName + "\" could not be read. " + failure.what(), failure.getDetails()};
}

void ShardReader::readChunk(const Catalog::Chunk& chunk, std::vector<std::uint8_t>& scratch, std::vector<std::uint8_t>& plain) const {
    const ShardIndex::Entry* entry = index.find(chunk.storedId);
    if (entry == nullptr) {
        throw Error(Error::Code::MissingChunk, "The shard \"" + name + "\" does not hold the chunk \"" + chunk.storedId.toHex() + "\" that its manifest places in it.");
    }

    scratch.resize(static_cast<std::size_t>(ChunkRecord::kHeaderSize + entry->encodedSize));
    file->readExactly(entry->recordOffset, scratch);
    std::optional<ChunkRecord> parsed;
    try {
        parsed = ChunkRecord::parse(std::span(scratch).first<ChunkRecord::kHeaderSize>());
    } catch (const Error& failure) {
        throw nameFile(failure, name);
    }

    const ChunkRecord& record = *parsed;

    // The record must be the chunk that the signed catalog names, with the sizes and the codec the catalog gives it, so a shard that someone rewrote with the content key still cannot change what an app reads, and a reader never trusts a size the record does not have.
    const bool matchesCatalog = record.describes(chunk);
    const bool matchesIndex = record.codec == entry->codec && record.profile == entry->profile && record.plainSize == entry->plainSize && record.encodedSize == entry->encodedSize;
    if (!matchesCatalog || !matchesIndex) {
        throw Error(Error::Code::CorruptChunk, "The chunk \"" + chunk.storedId.toHex() + "\" of the shard \"" + name + "\" is damaged or was changed.");
    }

    try {
        record.open(*key, std::span(scratch).subspan(ChunkRecord::kHeaderSize), plain);
    } catch (const Error& failure) {
        throw nameFile(failure, name);
    }
}

} // namespace haylen::content
