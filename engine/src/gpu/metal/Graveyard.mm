#include "gpu/metal/Graveyard.hpp"

namespace haylen::gpu::metal {

void Graveyard::retire(id object) {
    if (object != nil) {
        entries.push_back({.object = object, .frame = frame});
    }
}

void Graveyard::release(std::uint64_t completed) {
    while (!entries.empty() && entries.front().frame <= completed) {
        entries.pop_front();
    }
}

} // namespace haylen::gpu::metal
