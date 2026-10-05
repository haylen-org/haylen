#include "content/ContentBuilder.hpp"

#include <algorithm>
#include <stdexcept>
#include <utility>

#include "content/format/CatalogWriter.hpp"
#include "content/format/ChunkRecord.hpp"
#include "content/format/Chunker.hpp"
#include "content/format/ShardIndex.hpp"

namespace haylen::content {

ContentBuilder::ContentBuilder(std::filesystem::path outputFolder, std::shared_ptr<const ContentKey> contentKey, std::uint64_t shardTarget) : folder(std::move(outputFolder)), key(std::move(contentKey)), target(shardTarget) {}

void ContentBuilder::reuse(const Catalog& catalog, std::span<const ShardReference> earlierShards) {
    const auto first = static_cast<std::uint32_t>(shards.size());
    for (std::uint64_t index = 0; index < catalog.getChunkCount(); ++index) {
        Catalog::Chunk chunk = catalog.getChunk(index);
        chunk.shard += first;
        chunksByContent.try_emplace(chunk.contentId, chunk);
    }
    shards.insert(shards.end(), earlierShards.begin(), earlierShards.end());
}

ContentBuilder::Result ContentBuilder::build(const io::Package& source, std::vector<Input> inputs) {
    // Files go in path order, so the shards come out the same whatever order the inputs arrive in.
    std::ranges::sort(inputs, {}, &Input::path);
    if (const auto repeated = std::ranges::adjacent_find(inputs, {}, &Input::path); repeated != inputs.end()) {
        throw std::invalid_argument("The content build lists the file \"" + repeated->path + "\" twice.");
    }
    for (const Input& input : inputs) {
        addFile(source, input);
    }
    if (writer) {
        closeShard();
    }

    std::unordered_map<Digest, const Catalog::Chunk*, Digest::Hash> chunksByStored;
    for (const auto& [contentId, chunk] : chunksByContent) {
        chunksByStored.try_emplace(chunk.storedId, &chunk);
    }

    // Only the shards that the files still use stay in the list, earlier ones first.
    std::vector<bool> used(shards.size());
    for (const File& file : files) {
        for (const Digest& part : file.parts) {
            used[chunksByStored.at(part)->shard] = true;
        }
    }
    Result result;
    std::vector<std::uint32_t> positions(shards.size());
    for (std::size_t shard = 0; shard < shards.size(); ++shard) {
        if (used[shard]) {
            positions[shard] = static_cast<std::uint32_t>(result.shards.size());
            result.shards.push_back(shards[shard]);
        }
    }

    CatalogWriter catalog;
    for (const File& file : files) {
        for (const Digest& part : file.parts) {
            Catalog::Chunk chunk = *chunksByStored.at(part);
            chunk.shard = positions[chunk.shard];
            catalog.addChunk(chunk);
        }
        catalog.addFile(file.path, file.delivery, file.parts);
    }
    result.catalog = catalog.write();
    result.statistics = statistics;
    return result;
}

void ContentBuilder::addFile(const io::Package& source, const Input& input) {
    const std::unique_ptr<io::PackageReader> reader = source.openReader(input.path);
    const std::uint64_t size = reader->getSize();
    File file{.path = input.path, .delivery = input.delivery};
    fileInShard = false;

    // The window holds the next bytes of the file, at most one maximum chunk of them, and slides forward chunk by chunk.
    std::vector<std::uint8_t> window(static_cast<std::size_t>(std::min<std::uint64_t>(size, Chunker::kMaximumSize)));
    std::size_t begin = 0;
    std::size_t end = 0;
    for (std::uint64_t offset = 0; offset < size;) {
        const auto needed = static_cast<std::size_t>(std::min<std::uint64_t>(Chunker::kMaximumSize, size - offset));
        if (end - begin < needed) {
            std::copy(window.begin() + static_cast<std::ptrdiff_t>(begin), window.begin() + static_cast<std::ptrdiff_t>(end), window.begin());
            end -= begin;
            begin = 0;
            reader->readExactly(offset + end, std::span(window).subspan(end, needed - end));
            end = needed;
        }

        const std::span<const std::uint8_t> available = std::span(window).subspan(begin, end - begin);
        const std::size_t cut = Chunker::isSingleChunk(size) ? available.size() : Chunker::findCut(available);
        file.parts.push_back(addChunk(available.first(cut), size));
        begin += cut;
        offset += cut;
    }
    files.push_back(std::move(file));
}

Digest ContentBuilder::addChunk(std::span<const std::uint8_t> plain, std::uint64_t fileSize) {
    const Digest contentId = ChunkRecord::identifyContent(plain);
    if (const auto found = chunksByContent.find(contentId); found != chunksByContent.end()) {
        ++statistics.reusedChunks;
        statistics.reusedBytes += plain.size();
        return found->second.storedId;
    }

    std::vector<std::uint8_t> ciphertext;
    const ChunkRecord record = ChunkRecord::seal(*key, plain, contentId, ciphertext);

    // A shard closes before a record would take it past the target, unless the record continues a file small enough to stay whole in it.
    const bool keepsFileWhole = fileInShard && fileSize <= target / kWholeFileDivisor;
    if (writer && (writer->getEntryCount() >= ShardIndex::kMaximumEntries || (writer->getSize() + record.getRecordSize() + ShardIndex::kEntrySize > target && !keepsFileWhole))) {
        closeShard();
    }
    if (!writer) {
        writer.emplace(folder, key);
        fileInShard = false;
    }
    writer->add(record, ciphertext);
    fileInShard = true;

    chunksByContent.emplace(contentId, Catalog::Chunk{.storedId = record.storedId, .contentId = contentId, .plainSize = record.plainSize, .encodedSize = record.encodedSize, .shard = static_cast<std::uint32_t>(shards.size()), .codec = record.codec, .profile = record.profile});
    ++statistics.newChunks;
    statistics.newBytes += record.plainSize;
    statistics.storedBytes += record.encodedSize;
    return record.storedId;
}

void ContentBuilder::closeShard() {
    shards.push_back(writer->finish());
    writer.reset();
}

} // namespace haylen::content
