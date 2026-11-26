#pragma once

#include <Jolt/Jolt.h>

#include <Jolt/Core/TempAllocator.h>

#include <atomic>
#include <cstddef>

namespace haylen::physics3d::jolt {

// The memory that one step of a world works in, a block of the size of its settings. A step that needs more allocates the rest and records that it did, so the world reports the limit once instead of the step failing.
class StepAllocator final : public JPH::TempAllocator {
  public:
    explicit StepAllocator(std::size_t size);

    void* Allocate(JPH::uint size) noexcept override;
    void Free(void* address, JPH::uint size) noexcept override;

    // Tells whether a step needed more than the block since the last call.
    [[nodiscard]] bool takeOverflow() noexcept;

    // The most bytes of the block that any step used.
    [[nodiscard]] std::size_t getPeak() const noexcept {
        return peak.load(std::memory_order_relaxed);
    }

  private:
    JPH::TempAllocatorImpl block;
    std::atomic<std::size_t> peak = 0;
    std::atomic<bool> overflow = false;
};

} // namespace haylen::physics3d::jolt
