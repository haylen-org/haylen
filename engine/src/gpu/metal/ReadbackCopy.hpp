#pragma once

#import <Metal/Metal.h>

#include <atomic>
#include <cstddef>
#include <memory>
#include <optional>
#include <span>

namespace haylen::gpu::metal {

// A copy into a buffer the CPU reads, which a blit of its own command buffer makes after the frames committed before it, and whose completion handler marks it done. The caller records the copy into the blit encoder and submits it once.
class ReadbackCopy final {
  public:
    // Makes the buffer of the size in shared storage and the command buffer of the copy. Throws `std::runtime_error` when the GPU has no memory for the buffer.
    ReadbackCopy(id<MTLCommandQueue> queue, std::size_t size);

    [[nodiscard]] id<MTLBlitCommandEncoder> getBlit() const noexcept {
        return blit;
    }

    [[nodiscard]] id<MTLBuffer> getBuffer() const noexcept {
        return buffer;
    }

    // Ends the blit and commits its command buffer.
    void submit();

    // The bytes of the buffer once the GPU finished the copy, handed over once.
    [[nodiscard]] std::optional<std::span<const std::byte>> take() noexcept;

  private:
    id<MTLBuffer> buffer = nil;
    id<MTLCommandBuffer> commands = nil;
    id<MTLBlitCommandEncoder> blit = nil;
    std::shared_ptr<std::atomic<bool>> done = std::make_shared<std::atomic<bool>>(false);
    bool delivered = false;
};

} // namespace haylen::gpu::metal
