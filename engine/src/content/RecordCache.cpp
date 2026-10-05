#include "content/RecordCache.hpp"

#include <monocypher.h>

#include <fstream>
#include <iterator>
#include <stdexcept>
#include <utility>

#include "content/Error.hpp"
#include "content/format/Compression.hpp"

namespace haylen::content {

RecordCache::RecordCache(std::filesystem::path cacheFolder) : folder(std::move(cacheFolder)) {}

std::filesystem::path RecordCache::locate(const ContentKey& key, const Digest& contentId) const {
    const std::string name = contentId.toHex();
    return folder / key.getId().toHex() / Compression::getEncoderName() / name.substr(0, 2) / name;
}

std::optional<ChunkRecord> RecordCache::find(const ContentKey& key, const Digest& contentId, std::vector<std::uint8_t>& ciphertext) const {
    std::ifstream stream(locate(key, contentId), std::ios::binary);
    if (!stream) {
        return std::nullopt;
    }
    std::vector<std::uint8_t> bytes{std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
    if (bytes.size() < ChunkRecord::kHeaderSize) {
        return std::nullopt;
    }

    // An entry serves only when it is the record of the chunk and opens under the key, which checks every byte of it.
    try {
        const ChunkRecord record = ChunkRecord::parse(std::span(bytes).first<ChunkRecord::kHeaderSize>());
        if (record.contentId != contentId || bytes.size() != record.getRecordSize()) {
            return std::nullopt;
        }
        std::vector<std::uint8_t> opened(bytes.begin() + ChunkRecord::kHeaderSize, bytes.end());
        std::vector<std::uint8_t> plain;
        record.open(key, opened, plain);
        crypto_wipe(plain.data(), plain.size());
        ciphertext.assign(bytes.begin() + ChunkRecord::kHeaderSize, bytes.end());
        return record;
    } catch (const Error&) {
        return std::nullopt;
    }
}

void RecordCache::store(const ContentKey& key, const ChunkRecord& record, std::span<const std::uint8_t> ciphertext) const {
    const std::filesystem::path path = locate(key, record.contentId);
    std::filesystem::create_directories(path.parent_path());

    // The entry takes its name only once it is complete, so an interrupted build never leaves half a record behind it.
    std::filesystem::path partial = path;
    partial += ".partial";
    const ChunkRecord::Header header = record.serialize();
    std::ofstream stream(partial, std::ios::binary | std::ios::trunc);
    stream.write(reinterpret_cast<const char*>(header.data()), static_cast<std::streamsize>(header.size()));
    stream.write(reinterpret_cast<const char*>(ciphertext.data()), static_cast<std::streamsize>(ciphertext.size()));
    stream.close();
    if (!stream) {
        throw std::runtime_error("The build cache entry \"" + partial.generic_string() + "\" could not be written.");
    }
    std::filesystem::rename(partial, path);
}

} // namespace haylen::content
