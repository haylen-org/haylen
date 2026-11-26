#include "gpu/d3d11/BufferReadback.hpp"

#include <cstring>
#include <utility>

namespace haylen::gpu::d3d11 {

BufferReadback::BufferReadback(StagingCopy copy, std::size_t bytes) : staging(std::move(copy)), size(bytes) {}

std::optional<std::vector<std::uint8_t>> BufferReadback::poll() {
    if (delivered) {
        return std::nullopt;
    }

    const std::optional<D3D11_MAPPED_SUBRESOURCE> mapped = staging.map("The GPU could not hand over the bytes of a buffer.");
    if (!mapped) {
        return std::nullopt;
    }

    std::vector<std::uint8_t> bytes(size);
    std::memcpy(bytes.data(), mapped->pData, size);
    staging.unmap();
    delivered = true;
    return bytes;
}

} // namespace haylen::gpu::d3d11
