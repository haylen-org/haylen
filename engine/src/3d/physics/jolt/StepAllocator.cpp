#include "3d/physics/jolt/StepAllocator.hpp"

#include <Jolt/Core/Memory.h>

namespace haylen::physics3d::jolt {

StepAllocator::StepAllocator(std::size_t size) : block(size) {}

void* StepAllocator::Allocate(JPH::uint size) noexcept {
    if (!block.CanAllocate(size)) {
        overflow.store(true, std::memory_order_relaxed);
        return JPH::AlignedAllocate(size, JPH_RVECTOR_ALIGNMENT);
    }

    void* address = block.Allocate(size);
    const std::size_t used = block.GetUsage();
    std::size_t known = peak.load(std::memory_order_relaxed);
    while (used > known && !peak.compare_exchange_weak(known, used, std::memory_order_relaxed)) {}

    return address;
}

void StepAllocator::Free(void* address, JPH::uint size) noexcept {
    if (address != nullptr && !block.OwnsMemory(address)) {
        JPH::AlignedFree(address);
        return;
    }

    block.Free(address, size);
}

bool StepAllocator::takeOverflow() noexcept {
    return overflow.exchange(false, std::memory_order_relaxed);
}

} // namespace haylen::physics3d::jolt
