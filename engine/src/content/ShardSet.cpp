#include "content/ShardSet.hpp"

#include <utility>

#include "content/Error.hpp"

namespace haylen::content {

ShardSet::ShardSet(std::vector<ShardReference> shardReferences, std::shared_ptr<const KeyRing> contentKeys, Opener shardOpener) : references(std::move(shardReferences)), keys(std::move(contentKeys)), opener(std::move(shardOpener)), slots(references.size()) {}

const ShardReader& ShardSet::open(std::uint32_t shard) const {
    // A shard that fails to open stays closed, so a later read tries again.
    Slot& slot = slots[shard];
    std::scoped_lock lock(slot.mutex);
    if (slot.reader == nullptr) {
        const ShardReference& reference = references[shard];
        std::unique_ptr<io::PackageReader> file = opener(reference);
        if (file == nullptr) {
            throw Error(Error::Code::MissingShard, "The shard file \"" + reference.getFileName() + "\" is missing, so the app needs to be installed again.");
        }
        slot.reader = std::make_unique<const ShardReader>(std::move(file), reference, *keys);
    }
    return *slot.reader;
}

void ShardSet::readChunk(const Catalog::Chunk& chunk, std::vector<std::uint8_t>& scratch, std::vector<std::uint8_t>& plain) const {
    open(chunk.shard).readChunk(chunk.storedId, chunk.contentId, scratch, plain);
}

} // namespace haylen::content
