#include "gpu/webgpu/UploadRing.hpp"

#include <algorithm>
#include <cstring>

#include "gpu/webgpu/Conversion.hpp"

namespace haylen::gpu::webgpu {

UploadRing::UploadRing(WGPUDevice owner, WGPUBufferUsage usage, std::uint32_t offsetAlignment, std::string_view label) : device(owner), usages(usage), alignment(offsetAlignment), name(label) {
    chunks.push_back(makeChunk(kChunkSize));
}

UploadRing::~UploadRing() {
    for (const Chunk& chunk : chunks) {
        releaseChunk(chunk);
    }
}

// Queue writes take sizes in multiples of four bytes, so a chunk is as large as its pushes rounded up to whole words.
UploadRing::Chunk UploadRing::makeChunk(std::size_t size) const {
    const std::size_t bytes = std::max(kChunkSize, (size + 3U) & ~std::size_t{3U});
    WGPUBufferDescriptor descriptor = WGPU_BUFFER_DESCRIPTOR_INIT;
    descriptor.label = Conversion::toText(name);
    descriptor.usage = usages | WGPUBufferUsage_CopyDst;
    descriptor.size = bytes;
    return {.buffer = wgpuDeviceCreateBuffer(device, &descriptor), .bytes = std::vector<std::byte>(bytes)};
}

// WebGPU keeps the memory of a destroyed buffer until the work that the queue already took from it finished.
void UploadRing::releaseChunk(const Chunk& chunk) noexcept {
    wgpuBufferDestroy(chunk.buffer);
    wgpuBufferRelease(chunk.buffer);
}

bool UploadRing::fits(std::size_t size) const noexcept {
    const Chunk& chunk = chunks[current];
    const std::size_t start = (chunk.used + alignment - 1U) / alignment * alignment;
    return start + size <= chunk.bytes.size();
}

// No push of this frame reached a chunk past the current one, so a chunk that is too small is made again before anything points at it.
void UploadRing::advance(std::size_t size) {
    ++current;
    if (current == chunks.size()) {
        chunks.push_back(makeChunk(size));
        return;
    }

    if (chunks[current].bytes.size() < size) {
        releaseChunk(chunks[current]);
        chunks[current] = makeChunk(size);
    }
}

std::uint32_t UploadRing::push(std::span<const std::byte> data) noexcept {
    Chunk& chunk = chunks[current];
    const std::size_t start = (chunk.used + alignment - 1U) / alignment * alignment;
    std::memcpy(chunk.bytes.data() + start, data.data(), data.size());
    chunk.used = start + data.size();
    return static_cast<std::uint32_t>(start);
}

std::span<const std::byte> UploadRing::view(std::uint32_t chunk, std::uint32_t offset, std::uint32_t size) const noexcept {
    return std::span<const std::byte>(chunks[chunk].bytes).subspan(offset, size);
}

std::uint32_t UploadRing::getChunk() const noexcept {
    return current;
}

WGPUBuffer UploadRing::getBuffer(std::uint32_t chunk) const noexcept {
    return chunks[chunk].buffer;
}

// Queue writes take sizes in multiples of four bytes, which the chunk always has past what a frame used.
void UploadRing::flush(WGPUQueue queue) noexcept {
    for (std::uint32_t index = 0; index <= current; ++index) {
        Chunk& chunk = chunks[index];
        if (chunk.used != 0) {
            wgpuQueueWriteBuffer(queue, chunk.buffer, 0, chunk.bytes.data(), (chunk.used + 3U) & ~std::size_t{3U});
        }

        chunk.used = 0;
    }

    reached = current + 1U;
    current = 0;
}

std::size_t UploadRing::trim() noexcept {
    while (chunks.size() > reached) {
        releaseChunk(chunks.back());
        chunks.pop_back();
    }

    return chunks.size();
}

std::size_t UploadRing::getSpareBytes() const noexcept {
    std::size_t spare = 0;
    for (std::size_t index = reached; index < chunks.size(); ++index) {
        spare += chunks[index].bytes.size();
    }

    return spare;
}

} // namespace haylen::gpu::webgpu
