#include "content/ShardSet.hpp"

#include <algorithm>
#include <utility>

#include "haylen/content/Error.hpp"

namespace haylen::content {

ShardSet::ShardSet(std::vector<ShardReference> shardReferences, std::shared_ptr<const KeyRing> contentKeys, Opener shardOpener) : references(std::move(shardReferences)), keys(std::move(contentKeys)), opener(std::move(shardOpener)), readers(references.size()) {
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
std::shared_ptr<const ShardReader> ShardSet::open(std::uint32_t shard) const {
    if (std::shared_ptr<const ShardReader> reader = findOpen(shard)) {
        return reader;
    }
    const std::scoped_lock openingLock(opening[shard % kOpeningStripes]);
    if (std::shared_ptr<const ShardReader> reader = findOpen(shard)) {
        return reader;
    }

    const ShardReference& reference = references[shard];
    std::unique_ptr<io::PackageReader> file = opener(reference);
    if (file == nullptr) {
        throw Error(Error::Code::MissingShard, "The shard file \"" + reference.getFileName() + "\" is missing, so the app needs to be installed again.");
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

void ShardSet::readChunk(const Catalog::Chunk& chunk, std::vector<std::uint8_t>& scratch, std::vector<std::uint8_t>& plain) const {
    open(chunk.shard)->readChunk(chunk, scratch, plain);
}

} // namespace haylen::content
