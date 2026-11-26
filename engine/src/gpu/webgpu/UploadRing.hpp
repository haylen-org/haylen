#pragma once

#include <webgpu/webgpu.h>

#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

#include "haylen/gpu/Device.hpp"

namespace haylen::gpu::webgpu {

// The bytes that a frame uploads, such as the uniforms of its draws and the writes into its dynamic buffers, which go into memory of the CPU at aligned offsets and into buffers of the GPU with one queue write per buffer when the frame is submitted. WebGPU runs the queue writes of a submit before its commands and orders them with the submits before, so the commands of a frame read what the frame pushed, the writes of a frame never touch what an earlier frame still reads, and the buffers serve every frame. The buffers are chunks that keep their place, so the bind groups and the copies of earlier pushes keep pointing at their buffer when a frame needs more room.
class UploadRing final {
  public:
    // The bytes of a chunk, and the largest uniform block that a chunk of uniforms takes. A chunk holds every block of a program and one more at their largest, with the padding of the largest alignment of WebGPU before each, so the blocks that move on to a new chunk always fit. A push larger than a chunk, which only a write into a buffer makes, moves on to a chunk made as large as it.
    static constexpr std::size_t kChunkSize = std::size_t{1} << 20;
    static constexpr std::size_t kMaxBlockSize = std::size_t{64} << 10;
    static_assert((Device::kMaxBlocks + 1) * (kMaxBlockSize + 256) <= kChunkSize);

    // A ring whose buffers serve the usage and whose pushes start at multiples of the alignment.
    UploadRing(WGPUDevice owner, WGPUBufferUsage usage, std::uint32_t offsetAlignment, std::string_view label);
    UploadRing(const UploadRing&) = delete;
    UploadRing& operator=(const UploadRing&) = delete;
    ~UploadRing();

    // Whether the current chunk has room for the bytes.
    [[nodiscard]] bool fits(std::size_t size) const noexcept;

    // Moves on to the next chunk, making it the first time a frame reaches it, or again when it is smaller than the bytes it has to take.
    void advance(std::size_t size);

    // Copies the bytes into the current chunk and returns their offset in it.
    [[nodiscard]] std::uint32_t push(std::span<const std::byte> data) noexcept;

    // The bytes of an earlier push into a chunk of this frame.
    [[nodiscard]] std::span<const std::byte> view(std::uint32_t chunk, std::uint32_t offset, std::uint32_t size) const noexcept;

    [[nodiscard]] std::uint32_t getChunk() const noexcept;
    [[nodiscard]] WGPUBuffer getBuffer(std::uint32_t chunk) const noexcept;

    // Writes the pushes of the frame into the buffers of the GPU, before the frame is submitted, and starts the next frame from the first chunk.
    void flush(WGPUQueue queue) noexcept;

    // Releases the chunks beyond the ones that the last frame reached, and returns how many chunks stay, so the device lets go of the bind groups of the others.
    std::size_t trim() noexcept;

    // The bytes of the chunks beyond the ones that the last frame reached.
    [[nodiscard]] std::size_t getSpareBytes() const noexcept;

  private:
    struct Chunk {
        WGPUBuffer buffer = nullptr;
        std::vector<std::byte> bytes;
        std::size_t used = 0;
    };

    [[nodiscard]] Chunk makeChunk(std::size_t size) const;
    static void releaseChunk(const Chunk& chunk) noexcept;

    WGPUDevice device;
    WGPUBufferUsage usages;
    std::uint32_t alignment;
    std::string_view name;
    std::vector<Chunk> chunks;
    std::uint32_t current = 0;
    // The chunks that the last frame reached.
    std::size_t reached = 1;
};

} // namespace haylen::gpu::webgpu
