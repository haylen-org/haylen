#include "gpu/metal/Graveyard.hpp"

#include <algorithm>

namespace haylen::gpu::metal {

void Graveyard::reserve(std::size_t living) {
    const std::size_t needed = entries.size() + living;
    if (entries.capacity() < needed) {
        entries.reserve(std::max(needed, entries.capacity() * 2));
    }
}

void Graveyard::retire(id object) noexcept {
    if (object != nil) {
        entries.push_back({.object = object, .frame = frame});
    }
}

void Graveyard::release(std::uint64_t completed) {
    const auto pending = std::ranges::find_if(entries, [completed](const Entry& entry) { return entry.frame > completed; });
    entries.erase(entries.begin(), pending);
}

} // namespace haylen::gpu::metal
