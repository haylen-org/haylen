#pragma once

#include <cstddef>

namespace haylen::physics3d::jolt {

// Prepares Jolt once for the process. Its memory goes through the allocation functions of C++, so the allocations of the engine count it and a failure to allocate ends the app with its reason, and its types register for the shapes of the worlds.
class Library final {
  public:
    static void initialize();

  private:
    [[nodiscard]] static bool setUp();
    [[nodiscard]] static void* allocate(std::size_t size) noexcept;
    [[nodiscard]] static void* reallocate(void* block, std::size_t oldSize, std::size_t newSize) noexcept;
    static void release(void* block) noexcept;
    [[nodiscard]] static void* allocateAligned(std::size_t size, std::size_t alignment) noexcept;
    static void releaseAligned(void* block) noexcept;
};

} // namespace haylen::physics3d::jolt
