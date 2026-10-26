#include "gpu/webgpu/UniformRing.hpp"

#include <cstring>

#include "gpu/webgpu/Conversion.hpp"

namespace haylen::gpu::webgpu {

UniformRing::UniformRing(WGPUDevice owner, std::uint32_t offsetAlignment) : device(owner), alignment(offsetAlignment) {
    addChunk();
}

UniformRing::~UniformRing() {
    for (const Chunk& chunk : chunks) {
        wgpuBufferDestroy(chunk.buffer);
        wgpuBufferRelease(chunk.buffer);
    }
}

void UniformRing::addChunk() {
    WGPUBufferDescriptor descriptor = WGPU_BUFFER_DESCRIPTOR_INIT;
    descriptor.label = Conversion::toText("haylen-uniforms");
    descriptor.usage = WGPUBufferUsage_Uniform | WGPUBufferUsage_CopyDst;
    descriptor.size = kChunkSize;
    chunks.push_back({.buffer = wgpuDeviceCreateBuffer(device, &descriptor), .bytes = std::vector<std::byte>(kChunkSize)});
}

bool UniformRing::fits(std::size_t size) const noexcept {
    const std::size_t start = (chunks[current].used + alignment - 1U) / alignment * alignment;
    return start + size <= kChunkSize;
}

void UniformRing::advance() {
    ++current;
    if (current == chunks.size()) {
        addChunk();
    }
}

std::uint32_t UniformRing::push(std::span<const std::byte> data) noexcept {
    Chunk& chunk = chunks[current];
    const std::size_t start = (chunk.used + alignment - 1U) / alignment * alignment;
    std::memcpy(chunk.bytes.data() + start, data.data(), data.size());
    chunk.used = start + data.size();
    return static_cast<std::uint32_t>(start);
}

std::span<const std::byte> UniformRing::view(std::uint32_t chunk, std::uint32_t offset, std::uint32_t size) const noexcept {
    return std::span<const std::byte>(chunks[chunk].bytes).subspan(offset, size);
}

std::uint32_t UniformRing::getChunk() const noexcept {
    return current;
}

WGPUBuffer UniformRing::getBuffer(std::uint32_t chunk) const noexcept {
    return chunks[chunk].buffer;
}

std::size_t UniformRing::getChunkCount() const noexcept {
    return chunks.size();
}

// Queue writes take sizes in multiples of four bytes, which the chunk always has past what a frame used.
void UniformRing::flush(WGPUQueue queue) noexcept {
    for (std::uint32_t index = 0; index <= current; ++index) {
        Chunk& chunk = chunks[index];
        if (chunk.used != 0) {
            wgpuQueueWriteBuffer(queue, chunk.buffer, 0, chunk.bytes.data(), (chunk.used + 3U) & ~std::size_t{3U});
        }
        chunk.used = 0;
    }
    current = 0;
}

} // namespace haylen::gpu::webgpu
