#include "content/ShardSet.hpp"

#include <algorithm>
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
    if (const std::shared_ptr<const ShardReader> reader = findShard(chunk.shard)) {
        reader->readChunk(chunk, scratch, plain);
        return;
    }
    if (!store) {
        throw Error(Error::Code::MissingShard, "The shard file \"" + references[chunk.shard].getFileName() + "\" is missing, so the app needs to be installed again.");
    }
    readRecord(chunk, scratch, plain);
}

void ShardSet::readRecord(const Catalog::Chunk& chunk, std::vector<std::uint8_t>& scratch, std::vector<std::uint8_t>& plain) const {
    Digest keyId;
    if (!store->readRecord(chunk.storedId, scratch, keyId)) {
        throw Error(Error::Code::MissingChunk, "The chunk \"" + chunk.storedId.toHex() + "\" is not on the device yet, so its file must be downloaded before it is read.");
    }

    // A record of the store passes the same checks as one of a shard against the signed catalog, and one that fails leaves the store so a download replaces it.
    try {
        const ChunkRecord record = ChunkRecord::parse(std::span(scratch).first<ChunkRecord::kHeaderSize>());
        const bool matchesCatalog = record.storedId == chunk.storedId && record.contentId == chunk.contentId && record.codec == chunk.codec && record.profile == chunk.profile && record.plainSize == chunk.plainSize && record.encodedSize == chunk.encodedSize;
        if (!matchesCatalog || scratch.size() != record.getRecordSize()) {
            throw Error(Error::Code::CorruptChunk, "The record does not match its catalog.");
        }
        record.open(*keys->get(keyId), std::span(scratch).subspan(ChunkRecord::kHeaderSize), plain);
    } catch (const Error& error) {
        store->removeRecord(chunk.storedId);
        throw Error(Error::Code::CacheCorrupt, "The downloaded chunk \"" + chunk.storedId.toHex() + "\" was damaged on the device, so it was removed and downloads again. " + error.what());
    }
}

} // namespace haylen::content
