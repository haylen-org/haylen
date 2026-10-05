#include "content/format/CatalogWriter.hpp"

#include <stdexcept>
#include <unordered_map>
#include <utility>

#include "content/format/BinaryWriter.hpp"

namespace haylen::content {

void CatalogWriter::addChunk(const Catalog::Chunk& chunk) {
    chunks.try_emplace(chunk.storedId, chunk);
}

void CatalogWriter::addFile(std::string path, Delivery delivery, std::span<const Digest> parts) {
    if (!Catalog::isValidPath(path)) {
        throw std::invalid_argument("The path \"" + path + "\" cannot name a file of a catalog.");
    }
    for (const Digest& part : parts) {
        if (!chunks.contains(part)) {
            throw std::invalid_argument("The file \"" + path + "\" uses the chunk \"" + part.toHex() + "\", which the catalog does not list.");
        }
    }
    if (!files.try_emplace(path, File{.delivery = delivery, .parts = {parts.begin(), parts.end()}}).second) {
        throw std::invalid_argument("The catalog lists the file \"" + path + "\" twice.");
    }
}

std::vector<std::uint8_t> CatalogWriter::write() const {
    std::unordered_map<Digest, std::uint64_t, Digest::Hash> rows;
    std::uint64_t partCount = 0;
    std::uint64_t stringSize = 0;
    for (const auto& [storedId, chunk] : chunks) {
        rows.emplace(storedId, rows.size());
    }
    for (const auto& [path, file] : files) {
        partCount += file.parts.size();
        stringSize += path.size();
    }

    BinaryWriter writer;
    writer.writeBytes(Catalog::kMagic).write(Catalog::kVersion).writeZeros(sizeof(std::uint16_t));
    writer.write(std::uint64_t{files.size()}).write(std::uint64_t{chunks.size()}).write(partCount).write(stringSize);
    writer.writeZeros(Catalog::kHeaderSize - writer.size());

    std::uint64_t pathOffset = 0;
    std::uint64_t firstPart = 0;
    for (const auto& [path, file] : files) {
        std::uint64_t size = 0;
        for (const Digest& part : file.parts) {
            size += chunks.at(part).plainSize;
        }
        writer.write(pathOffset).write(static_cast<std::uint32_t>(path.size())).write(static_cast<std::uint8_t>(Catalog::Kind::File)).write(static_cast<std::uint8_t>(file.delivery)).writeZeros(sizeof(std::uint16_t));
        writer.write(size).write(firstPart).write(std::uint64_t{file.parts.size()});
        pathOffset += path.size();
        firstPart += file.parts.size();
    }

    for (const auto& [storedId, chunk] : chunks) {
        writer.writeDigest(chunk.storedId).writeDigest(chunk.contentId).write(chunk.plainSize).write(chunk.encodedSize);
        writer.write(chunk.shard).write(static_cast<std::uint8_t>(chunk.codec)).write(chunk.profile).writeZeros(sizeof(std::uint16_t));
    }

    for (const auto& [path, file] : files) {
        std::uint64_t offset = 0;
        for (const Digest& part : file.parts) {
            writer.write(rows.at(part)).write(offset);
            offset += chunks.at(part).plainSize;
        }
    }

    for (const auto& [path, file] : files) {
        writer.writeBytes({reinterpret_cast<const std::uint8_t*>(path.data()), path.size()});
    }
    return writer.take();
}

} // namespace haylen::content
