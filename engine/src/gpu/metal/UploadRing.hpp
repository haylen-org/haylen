#pragma once

#import <Metal/Metal.h>

#include <cstddef>
#include <span>
#include <vector>

namespace haylen::gpu::metal {

// The memory of one frame in flight that uniforms and the pixels of dynamic images pass through on their way to the GPU: a buffer in storage the CPU and the GPU share, which the frame fills from its start and which grows to hold the largest frame. The GPU reads it until the frame finished, so a frame resets it only once the frame that used it before finished.
class UploadRing final {
  public:
    struct Allocation {
        __unsafe_unretained id<MTLBuffer> buffer = nil;
        NSUInteger offset = 0;
    };

    UploadRing(id<MTLDevice> device, std::size_t capacity);

    // Starts the frame from the start of the buffer, letting go of the smaller buffers the frame before outgrew.
    void reset() noexcept;

    // Copies the bytes at the next offset of the alignment and returns where they went.
    [[nodiscard]] Allocation write(std::span<const std::byte> data, std::size_t alignment);

    // Makes the buffer as small as the last frame of the slot wrote, never smaller than its first size, once the GPU finished that frame.
    void trim();

    // The bytes of the buffer beyond what the last frame of the slot wrote, with the buffers it outgrew.
    [[nodiscard]] std::size_t getSpareBytes() const noexcept;

  private:
    void grow(std::size_t needed);
    void replace(std::size_t size);
    [[nodiscard]] std::size_t getNeededCapacity() const noexcept;

    id<MTLDevice> device = nil;
    id<MTLBuffer> buffer = nil;
    std::byte* contents = nullptr;
    std::size_t initialCapacity = 0;
    std::size_t capacity = 0;
    std::size_t used = 0;
    std::vector<id<MTLBuffer>> outgrown;
};

} // namespace haylen::gpu::metal
