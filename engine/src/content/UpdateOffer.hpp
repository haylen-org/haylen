#pragma once

#include <cstdint>
#include <memory>
#include <vector>

#include "content/ReleasePackage.hpp"
#include "content/format/Catalog.hpp"

namespace haylen::content {

// Newer content that the channel of an app offers: its generation, the verified manifest with its catalog and its signed bytes, the chunks of its required files that the device lacks and the bytes an install downloads for them.
struct UpdateOffer {
    std::uint64_t generation = 0;
    std::shared_ptr<const ReleasePackage::Content> content;
    std::shared_ptr<const std::vector<std::uint8_t>> manifest;
    std::vector<Catalog::Chunk> missing;
    std::uint64_t downloadBytes = 0;
    std::uint64_t reusedBytes = 0;
};

} // namespace haylen::content
