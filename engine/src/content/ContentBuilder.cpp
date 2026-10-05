#include "content/ContentBuilder.hpp"

#include <algorithm>
#include <stdexcept>
#include <unordered_set>
#include <utility>

#include "content/format/CatalogWriter.hpp"
#include "content/format/ChunkRecord.hpp"
#include "content/format/Chunker.hpp"
#include "content/format/ShardIndex.hpp"

namespace haylen::content {

ContentBuilder::ContentBuilder(std::filesystem::path outputFolder, std::shared_ptr<const ContentKey> contentKey, std::uint64_t shardTarget, std::shared_ptr<const RecordCache> recordCache) : folder(std::move(outputFolder)), key(std::move(contentKey)), target(shardTarget), cache(std::move(recordCache)) {}

void ContentBuilder::reuse(const Catalog& catalog, std::span<const ShardReference> earlier) {
    const auto first = static_cast<std::uint32_t>(earlierShards.size());
    for (std::uint64_t index = 0; index < catalog.getChunkCount(); ++index) {
        Catalog::Chunk chunk = catalog.getChunk(index);
        chunk.shard += first;
        earlierChunks.try_emplace(chunk.contentId, chunk);
    }
    earlierShards.insert(earlierShards.end(), earlier.begin(), earlier.end());
}

ContentBuilder::Result ContentBuilder::build(const io::Package& source, std::vector<Input> inputs) {
    // Files go in path order, so the shards come out the same whatever order the inputs arrive in.
    std::ranges::sort(inputs, {}, &Input::path);
    if (const auto repeated = std::ranges::adjacent_find(inputs, {}, &Input::path); repeated != inputs.end()) {
        throw std::invalid_argument("The content build lists the file \"" + repeated->path + "\" twice.");
    }

    // Every file is split into chunks first, so the build knows which earlier shards its files still use before it writes anything.
    std::vector<File> files;
    files.reserve(inputs.size());
    for (Input& input : inputs) {
        files.push_back(identify(source, std::move(input)));
    }
    keepEarlierShards(files);

    for (File& file : files) {
        storeFile(source, file);
    }
    if (writer) {
        closeShard();
    }

    std::unordered_map<Digest, const Catalog::Chunk*, Digest::Hash> chunksByStored;
    for (const auto& [contentId, chunk] : chunksByContent) {
        chunksByStored.try_emplace(chunk.storedId, &chunk);
    }
    CatalogWriter catalog;
    for (const File& file : files) {
        for (const Digest& part : file.parts) {
            catalog.addChunk(*chunksByStored.at(part));
        }
        catalog.addFile(file.input.path, file.input.delivery, file.parts, file.input.kind);
    }
    return {.catalog = catalog.write(), .shards = shards, .keptShards = keptShards, .statistics = statistics};
}

ContentBuilder::File ContentBuilder::identify(const io::Package& source, Input input) {
    const std::unique_ptr<io::PackageReader> reader = source.openReader(input.path);
    File file{.input = std::move(input), .size = reader->getSize()};

    // The window holds the next bytes of the file, at most one maximum chunk of them, and slides forward chunk by chunk.
    std::vector<std::uint8_t> window(static_cast<std::size_t>(std::min<std::uint64_t>(file.size, Chunker::kMaximumSize)));
    std::size_t begin = 0;
    std::size_t end = 0;
    for (std::uint64_t offset = 0; offset < file.size;) {
        const auto needed = static_cast<std::size_t>(std::min<std::uint64_t>(Chunker::kMaximumSize, file.size - offset));
        if (end - begin < needed) {
            std::copy(window.begin() + static_cast<std::ptrdiff_t>(begin), window.begin() + static_cast<std::ptrdiff_t>(end), window.begin());
            end -= begin;
            begin = 0;
            reader->readExactly(offset + end, std::span(window).subspan(end, needed - end));
            end = needed;
        }

        const std::span<const std::uint8_t> available = std::span(window).subspan(begin, end - begin);
        const std::size_t cut = Chunker::isSingleChunk(file.size) ? available.size() : Chunker::findCut(available);
        file.pieces.push_back({.offset = offset, .size = cut, .contentId = ChunkRecord::identifyContent(available.first(cut))});
        begin += cut;
        offset += cut;
    }
    return file;
}

void ContentBuilder::keepEarlierShards(const std::vector<File>& files) {
    // The record bytes of each earlier shard that the files still use, counting each chunk once.
    std::vector<std::uint64_t> live(earlierShards.size());
    std::unordered_set<Digest, Digest::Hash> counted;
    for (const File& file : files) {
        for (const Piece& piece : file.pieces) {
            const auto found = earlierChunks.find(piece.contentId);
            if (found != earlierChunks.end() && counted.insert(piece.contentId).second) {
                live[found->second.shard] += ChunkRecord::kHeaderSize + found->second.encodedSize;
            }
        }
    }

    std::vector<std::optional<std::uint32_t>> positions(earlierShards.size());
    for (std::size_t shard = 0; shard < earlierShards.size(); ++shard) {
        if (live[shard] == 0 || live[shard] < earlierShards[shard].fileSize - earlierShards[shard].fileSize / 2) {
            ++statistics.droppedShards;
            continue;
        }
        positions[shard] = static_cast<std::uint32_t>(shards.size());
        shards.push_back(earlierShards[shard]);
    }
    keptShards = shards.size();

    for (const auto& [contentId, chunk] : earlierChunks) {
        if (const std::optional<std::uint32_t> position = positions[chunk.shard]) {
            Catalog::Chunk kept = chunk;
            kept.shard = *position;
            chunksByContent.emplace(contentId, kept);
        }
    }
}

void ContentBuilder::storeFile(const io::Package& source, File& file) {
    std::unique_ptr<io::PackageReader> reader;
    std::vector<std::uint8_t> plain;
    fileInShard = false;
    for (const Piece& piece : file.pieces) {
        if (const auto found = chunksByContent.find(piece.contentId); found != chunksByContent.end()) {
            file.parts.push_back(found->second.storedId);
            ++statistics.reusedChunks;
            statistics.reusedBytes += piece.size;
            continue;
        }
        if (!reader) {
            reader = source.openReader(file.input.path);
        }
        file.parts.push_back(storePiece(*reader, file, piece, plain));
    }
}

Digest ContentBuilder::storePiece(io::PackageReader& reader, const File& file, const Piece& piece, std::vector<std::uint8_t>& plain) {
    std::vector<std::uint8_t> ciphertext;
    std::optional<ChunkRecord> record = cache ? cache->find(*key, piece.contentId, ciphertext) : std::nullopt;
    if (record) {
        ++statistics.cachedChunks;
    } else {
        plain.resize(static_cast<std::size_t>(piece.size));
        reader.readExactly(piece.offset, plain);

        // A chunk is stored only with the bytes its ID names, so a file that changes while the release builds fails the build instead of corrupting it.
        if (ChunkRecord::identifyContent(plain) != piece.contentId) {
            throw std::runtime_error("The file \"" + file.input.path + "\" changed while the release was built. Build the release again.");
        }
        record = ChunkRecord::seal(*key, plain, piece.contentId, ciphertext);
        if (cache) {
            cache->store(*key, *record, ciphertext);
        }
    }

    // A shard closes before a record would take it past the target, unless the record continues a file small enough to stay whole in it.
    const bool keepsFileWhole = fileInShard && file.size <= target / kWholeFileDivisor;
    if (writer && (writer->getEntryCount() >= ShardIndex::kMaximumEntries || (writer->getSize() + record->getRecordSize() + ShardIndex::kEntrySize > target && !keepsFileWhole))) {
        closeShard();
    }
    if (!writer) {
        writer.emplace(folder, key);
        fileInShard = false;
    }
    writer->add(*record, ciphertext);
    fileInShard = true;

    chunksByContent.emplace(piece.contentId, Catalog::Chunk{.storedId = record->storedId, .contentId = piece.contentId, .plainSize = record->plainSize, .encodedSize = record->encodedSize, .shard = static_cast<std::uint32_t>(shards.size()), .codec = record->codec, .profile = record->profile});
    ++statistics.newChunks;
    statistics.newBytes += record->plainSize;
    statistics.storedBytes += record->encodedSize;
    return record->storedId;
}

void ContentBuilder::closeShard() {
    shards.push_back(writer->finish());
    writer.reset();
}

} // namespace haylen::content
