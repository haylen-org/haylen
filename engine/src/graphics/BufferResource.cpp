#include "graphics/BufferResource.hpp"

namespace haylen::graphics {

debug::ObjectCounter& BufferResource::counter = *new debug::ObjectCounter("Buffer", debug::ObjectCounter::Kind::Native);

BufferResource::~BufferResource() {
    if (const auto owner = graveyard.lock()) {
        owner->bury(buffer);
    }
}

} // namespace haylen::graphics
