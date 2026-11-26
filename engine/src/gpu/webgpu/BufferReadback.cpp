#include "gpu/webgpu/BufferReadback.hpp"

#include <cstring>
#include <span>

namespace haylen::gpu::webgpu {

BufferReadback::BufferReadback(WGPUDevice device, WGPUQueue queue, WGPUBuffer source, std::size_t offset, std::size_t size) : copy(device, size, "haylen-buffer-readback") {
    WGPUCommandEncoder encoder = wgpuDeviceCreateCommandEncoder(device, nullptr);
    wgpuCommandEncoderCopyBufferToBuffer(encoder, source, offset, copy.getBuffer(), 0, size);
    copy.submit(queue, encoder);
}

std::optional<std::vector<std::uint8_t>> BufferReadback::poll() {
    const std::optional<std::span<const std::byte>> mapped = copy.getMapped("The GPU could not hand over the bytes of a buffer.");
    if (!mapped) {
        return std::nullopt;
    }

    std::vector<std::uint8_t> bytes(mapped->size());
    std::memcpy(bytes.data(), mapped->data(), mapped->size());
    copy.deliver();
    return bytes;
}

} // namespace haylen::gpu::webgpu
