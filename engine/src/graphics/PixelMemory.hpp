#pragma once

#include <cstddef>

namespace haylen::graphics {

// The memory of the pixels of images, which the image decoder allocates through too, so a decoded image keeps the buffer the decoder filled instead of a copy of it. A decode caps every allocation of its thread at the largest buffer its image may need, so damaged data never grows a buffer without end.
class PixelMemory final {
  public:
    // Caps every allocation of the current thread at a number of bytes while it lives, and tells whether it refused one.
    class Cap final {
      public:
        explicit Cap(std::size_t bytes) noexcept;
        ~Cap();

        Cap(const Cap&) = delete;
        Cap& operator=(const Cap&) = delete;

        [[nodiscard]] bool hasRefused() const noexcept {
            return refused;
        }

      private:
        friend class PixelMemory;

        std::size_t limit;
        bool refused = false;
        Cap* previous;
    };

    // Returns a block of the size, or null when the memory runs out or the size passes the cap of the thread.
    [[nodiscard]] static void* allocate(std::size_t size) noexcept;

    // Moves a block into one of the new size with the bytes the two share, or returns null and keeps the block as it was.
    [[nodiscard]] static void* reallocate(void* block, std::size_t oldSize, std::size_t newSize) noexcept;

    static void release(void* block) noexcept;

  private:
    static thread_local Cap* active;
};

} // namespace haylen::graphics
