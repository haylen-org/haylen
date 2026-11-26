#include "gpu/metal/BufferReadback.hpp"

#include <cstring>
#include <span>

namespace haylen::gpu::metal {

BufferReadback::BufferReadback(id<MTLCommandQueue> queue, id<MTLBuffer> source, std::size_t offset, std::size_t size) : copy(queue, size) {
    [copy.getBlit() copyFromBuffer:source sourceOffset:offset toBuffer:copy.getBuffer() destinationOffset:0 size:size];
    copy.submit();
}

std::optional<std::vector<std::uint8_t>> BufferReadback::poll() {
    const std::optional<std::span<const std::byte>> bytes = copy.take();
    if (!bytes) {
        return std::nullopt;
    }

    std::vector<std::uint8_t> copied(bytes->size());
    std::memcpy(copied.data(), bytes->data(), copied.size());
    return copied;
}

} // namespace haylen::gpu::metal
