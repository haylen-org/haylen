#include "gpu/metal/ReadbackCopy.hpp"

#include <format>
#include <stdexcept>

namespace haylen::gpu::metal {

ReadbackCopy::ReadbackCopy(id<MTLCommandQueue> queue, std::size_t size) {
    buffer = [queue.device newBufferWithLength:size options:MTLResourceStorageModeShared];
    if (buffer == nil) {
        throw std::runtime_error(std::format("The GPU has no memory to read {} bytes back into.", size));
    }

    @autoreleasepool {
        commands = [queue commandBuffer];
        commands.label = @"haylen-readback";
        blit = [commands blitCommandEncoder];
    }
}

// A copy that failed, such as on a lost device, still ends, so the caller never waits forever.
void ReadbackCopy::submit() {
    [blit endEncoding];
    const std::shared_ptr<std::atomic<bool>> finished = done;
    @autoreleasepool {
        [commands addCompletedHandler:^(id<MTLCommandBuffer>) { finished->store(true, std::memory_order_release); }];
        [commands commit];
    }

    blit = nil;
    commands = nil;
}

std::optional<std::span<const std::byte>> ReadbackCopy::take() noexcept {
    if (delivered || !done->load(std::memory_order_acquire)) {
        return std::nullopt;
    }

    delivered = true;
    return std::span<const std::byte>(static_cast<const std::byte*>(buffer.contents), buffer.length);
}

} // namespace haylen::gpu::metal
