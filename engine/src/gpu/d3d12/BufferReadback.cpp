#include "gpu/d3d12/BufferReadback.hpp"

#include <cstring>
#include <span>
#include <utility>

namespace haylen::gpu::d3d12 {

BufferReadback::BufferReadback(ReadbackCopy copy, std::size_t bytes) : readback(std::move(copy)), size(bytes) {}

std::optional<std::vector<std::uint8_t>> BufferReadback::poll() {
    if (delivered) {
        return std::nullopt;
    }

    const std::optional<std::span<const std::byte>> mapped = readback.map(size, "The GPU could not hand over the bytes of a buffer.");
    if (!mapped) {
        return std::nullopt;
    }

    std::vector<std::uint8_t> bytes(size);
    std::memcpy(bytes.data(), mapped->data(), size);
    readback.unmap();
    delivered = true;
    return bytes;
}

} // namespace haylen::gpu::d3d12
