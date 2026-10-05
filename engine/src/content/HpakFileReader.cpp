#include "content/HpakFileReader.hpp"

#include <monocypher.h>

#include <algorithm>
#include <utility>

namespace haylen::content {

HpakFileReader::HpakFileReader(std::shared_ptr<const Catalog> fileCatalog, std::shared_ptr<const ShardSet> fileShards, const Catalog::File& catalogFile) : catalog(std::move(fileCatalog)), shards(std::move(fileShards)), file(catalogFile) {}

HpakFileReader::~HpakFileReader() {
    crypto_wipe(plain.data(), plain.size());
}

std::size_t HpakFileReader::read(std::uint64_t offset, std::span<std::uint8_t> target) {
    if (offset >= file.size || target.empty()) {
        return 0;
    }

    const auto count = static_cast<std::size_t>(std::min<std::uint64_t>(target.size(), file.size - offset));
    std::scoped_lock lock(mutex);
    std::size_t filled = 0;
    for (std::uint64_t part = catalog->findPart(file, offset); filled < count; ++part) {
        const Catalog::Part piece = catalog->getPart(part);
        if (cachedPart != part) {
            cachedPart.reset();
            shards->readChunk(catalog->getChunk(piece.chunk), scratch, plain);
            cachedPart = part;
        }

        const auto begin = static_cast<std::size_t>(offset + filled - piece.offset);
        const std::size_t length = std::min(plain.size() - begin, count - filled);
        std::copy_n(plain.begin() + static_cast<std::ptrdiff_t>(begin), length, target.begin() + static_cast<std::ptrdiff_t>(filled));
        filled += length;
    }
    return count;
}

} // namespace haylen::content
