#include "2d/graphics/StaticBatchResource.hpp"

namespace haylen::graphics2d {

debug::ObjectCounter& StaticBatchResource::counter = *new debug::ObjectCounter("StaticSpriteBatch", debug::ObjectCounter::Kind::Native);

StaticBatchResource::~StaticBatchResource() {
    if (const auto owner = graveyard.lock()) {
        owner->bury(buffer);
    }
}

} // namespace haylen::graphics2d
