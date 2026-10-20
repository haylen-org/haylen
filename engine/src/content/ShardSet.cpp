#include "content/ShardSet.hpp"

#include <algorithm>
#include <chrono>
#include <utility>

#include "content/format/ChunkRecord.hpp"
#include "haylen/content/Error.hpp"

namespace haylen::content {

ShardSet::ShardSet(std::vector<ShardReference> shardReferences, std::shared_ptr<const KeyRing> contentKeys, Opener shardOpener, std::shared_ptr<LocalStore> recordStore) : references(std::move(shardReferences)), keys(std::move(contentKeys)), opener(std::move(shardOpener)), store(std::move(recordStore)), readers(references.size()), missing(references.size()) {
    recent.reserve(kMaxOpenShards + 1);
}

std::shared_ptr<const ShardReader> ShardSet::findOpen(std::uint32_t shard) const {
    const std::scoped_lock lock(mutex);
    std::shared_ptr<const ShardReader> reader = readers[shard];
    if (reader != nullptr) {
        const auto used = std::ranges::find(recent, shard);
        std::rotate(used, used + 1, recent.end());
    }
    return reader;
}

// A shard that fails to open stays closed, so a later read tries again.
std::shared_ptr<const ShardReader> ShardSet::findShard(std::uint32_t shard) const {
    if (std::shared_ptr<const ShardReader> reader = findOpen(shard)) {
        return reader;
    }
    const std::scoped_lock openingLock(opening[shard % kOpeningStripes]);
    if (std::shared_ptr<const ShardReader> reader = findOpen(shard)) {
        return reader;
    }
    {
        const std::scoped_lock lock(mutex);
        if (missing[shard]) {
            return nullptr;
        }
    }

    const ShardReference& reference = references[shard];
    std::unique_ptr<io::PackageReader> file = opener(reference);
    if (file == nullptr) {
        const std::scoped_lock lock(mutex);
        missing[shard] = true;
        return nullptr;
    }
    auto reader = std::make_shared<const ShardReader>(std::move(file), reference, *keys);

    // The shard that closes lets go of its file outside the lock, once the reads that still hold it end.
    std::shared_ptr<const ShardReader> closing;
    const std::scoped_lock lock(mutex);
    readers[shard] = reader;
    recent.push_back(shard);
    if (recent.size() > kMaxOpenShards) {
        closing = std::exchange(readers[recent.front()], nullptr);
        recent.erase(recent.begin());
    }
    return reader;
}

bool ShardSet::hasChunk(const Catalog::Chunk& chunk) const {
    return (store && store->contains(chunk.storedId)) || findShard(chunk.shard) != nullptr;
}

void ShardSet::forgetMissing() const {
    const std::scoped_lock lock(mutex);
    missing.assign(missing.size(), false);
}

void ShardSet::readChunk(const Catalog::Chunk& chunk, std::vector<std::uint8_t>& scratch, std::vector<std::uint8_t>& plain) const {
    const auto started = std::chrono::steady_clock::now();
    activeReads.fetch_add(1, std::memory_order_relaxed);
    try {
        if (const std::shared_ptr<const ShardReader> reader = findShard(chunk.shard)) {
            reader->readChunk(chunk, scratch, plain);
        } else if (store) {
            readRecord(chunk, scratch, plain);
        } else {
            throw Error(Error::Code::MissingShard, "The shard file \"" + references[chunk.shard].getFileName() + "\" is missing, so the app needs to be installed again.");
        }
    } catch (Error& error) {
        activeReads.fetch_sub(1, std::memory_order_relaxed);
        Error::Details& details = error.getDetails();
        details.packId = references[chunk.shard].shardId.toHex();
        details.contentId = chunk.contentId.toHex();
        details.storedId = chunk.storedId.toHex();
        throw;
    } catch (...) {
        activeReads.fetch_sub(1, std::memory_order_relaxed);
        throw;
    }

    activeReads.fetch_sub(1, std::memory_order_relaxed);
    decodedChunks.fetch_add(1, std::memory_order_relaxed);
    decodedBytes.fetch_add(plain.size(), std::memory_order_relaxed);
    decodeNanoseconds.fetch_add(static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - started).count()), std::memory_order_relaxed);
}

ShardSet::Reads ShardSet::getReads() const {
    Reads reads{.decodedChunks = decodedChunks.load(std::memory_order_relaxed), .decodedBytes = decodedBytes.load(std::memory_order_relaxed), .decodeNanoseconds = decodeNanoseconds.load(std::memory_order_relaxed), .activeReads = activeReads.load(std::memory_order_relaxed)};
    const std::scoped_lock lock(mutex);
    reads.openShards = static_cast<std::uint64_t>(std::ranges::count_if(readers, [](const std::shared_ptr<const ShardReader>& reader) { return reader != nullptr; }));
    return reads;
}

void ShardSet::readRecord(const Catalog::Chunk& chunk, std::vector<std::uint8_t>& scratch, std::vector<std::uint8_t>& plain) const {
    Digest keyId;
    if (!store->readRecord(chunk.storedId, scratch, keyId)) {
        throw Error(Error::Code::MissingChunk, "The chunk \"" + chunk.storedId.toHex() + "\" is not on the device yet, so its file must be downloaded before it is read.");
    }

    // A record of the store passes the same checks as one of a shard against the signed catalog, and one that fails leaves the store so a download replaces it.
    try {
        const ChunkRecord record = ChunkRecord::parse(std::span(scratch).first<ChunkRecord::kHeaderSize>());
        if (!record.describes(chunk) || scratch.size() != record.getRecordSize()) {
            throw Error(Error::Code::CorruptChunk, "The record does not match its catalog.");
        }
        record.open(*keys->get(keyId), std::span(scratch).subspan(ChunkRecord::kHeaderSize), plain);
    } catch (const Error& error) {
        store->removeRecord(chunk.storedId);
        throw Error(Error::Code::CacheCorrupt, "The downloaded chunk \"" + chunk.storedId.toHex() + "\" was damaged on the device, so it was removed and downloads again. " + error.what());
    }
}

} // namespace haylen::content
