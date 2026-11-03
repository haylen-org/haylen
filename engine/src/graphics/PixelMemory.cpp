#include "graphics/PixelMemory.hpp"

#include <algorithm>
#include <cstring>
#include <new>

namespace haylen::graphics {

thread_local PixelMemory::Cap* PixelMemory::active = nullptr;

PixelMemory::Cap::Cap(std::size_t bytes) noexcept : limit(bytes), previous(active) {
    active = this;
}

PixelMemory::Cap::~Cap() {
    active = previous;
}

void* PixelMemory::allocate(std::size_t size) noexcept {
    if (active != nullptr && size > active->limit) {
        active->refused = true;
        return nullptr;
    }
    return ::operator new(size, std::nothrow);
}

void* PixelMemory::reallocate(void* block, std::size_t oldSize, std::size_t newSize) noexcept {
    void* moved = allocate(newSize);
    if (moved == nullptr) {
        return nullptr;
    }

    if (block != nullptr) {
        std::memcpy(moved, block, std::min(oldSize, newSize));
        release(block);
    }
    return moved;
}

void PixelMemory::release(void* block) noexcept {
    ::operator delete(block);
}

} // namespace haylen::graphics
