#include "3d/physics/jolt/Library.hpp"

#include <Jolt/Jolt.h>

#include <Jolt/Core/Factory.h>
#include <Jolt/Core/Memory.h>
#include <Jolt/RegisterTypes.h>

#include <algorithm>
#include <cstring>
#include <memory>
#include <new>

namespace haylen::physics3d::jolt {

void Library::initialize() {
    [[maybe_unused]] static const bool ready = setUp();
}

// The factory of Jolt lives for the whole process, since the shapes of any world may need it until the process ends.
bool Library::setUp() {
    JPH::Allocate = &allocate;
    JPH::Reallocate = &reallocate;
    JPH::Free = &release;
    JPH::AlignedAllocate = &allocateAligned;
    JPH::AlignedFree = &releaseAligned;
    JPH::Factory::sInstance = new JPH::Factory();
    JPH::RegisterTypes();
    return true;
}

void* Library::allocate(std::size_t size) noexcept {
    return ::operator new(size);
}

void* Library::reallocate(void* block, std::size_t oldSize, std::size_t newSize) noexcept {
    void* moved = allocate(newSize);
    if (block != nullptr) {
        std::memcpy(moved, block, std::min(oldSize, newSize));
        release(block);
    }

    return moved;
}

void Library::release(void* block) noexcept {
    ::operator delete(block);
}

// The block keeps the address of the allocation it lies in just before it, since Jolt frees an aligned block without its alignment.
void* Library::allocateAligned(std::size_t size, std::size_t alignment) noexcept {
    std::size_t space = size + alignment;
    void* base = ::operator new(space + sizeof(void*));
    void* block = static_cast<std::byte*>(base) + sizeof(void*);
    std::align(alignment, size, block, space);
    std::memcpy(static_cast<std::byte*>(block) - sizeof(void*), &base, sizeof(void*));
    return block;
}

void Library::releaseAligned(void* block) noexcept {
    if (block == nullptr) {
        return;
    }

    void* base = nullptr;
    std::memcpy(&base, static_cast<std::byte*>(block) - sizeof(void*), sizeof(void*));
    ::operator delete(base);
}

} // namespace haylen::physics3d::jolt
