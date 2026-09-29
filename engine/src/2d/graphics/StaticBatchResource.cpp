#include "2d/graphics/StaticBatchResource.hpp"

namespace haylen::graphics2d {

StaticBatchResource::~StaticBatchResource() {
    if (const auto owner = graveyard.lock()) {
        owner->buryBuffer(buffer);
    }
}

} // namespace haylen::graphics2d
