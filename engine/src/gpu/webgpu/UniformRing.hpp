#pragma once

#include <webgpu/webgpu.h>

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace haylen::gpu::webgpu {

// The uniforms of the draws of a frame, which go into memory of the CPU at the offsets that dynamic bindings reach and into uniform buffers of the GPU with one queue write per buffer when the frame is submitted. WebGPU orders queue writes with submissions, so the writes of a frame never touch what an earlier frame still reads, and the buffers serve every frame. The buffers are chunks of a fixed size, so the bind groups of earlier draws keep pointing at their buffer when a frame needs more room.
class UniformRing final {
  public:
    // The bytes of each chunk, which hold the largest uniform blocks of a program several times over.
    static constexpr std::size_t kChunkSize = std::size_t{1} << 20;

    UniformRing(WGPUDevice owner, std::uint32_t offsetAlignment);
    UniformRing(const UniformRing&) = delete;
    UniformRing& operator=(const UniformRing&) = delete;
    ~UniformRing();

    // Whether the current chunk has room for the bytes.
    [[nodiscard]] bool fits(std::size_t size) const noexcept;

    // Moves on to the next chunk, creating it the first time a frame reaches it.
    void advance();

    // Copies the bytes into the current chunk and returns their offset in it.
    [[nodiscard]] std::uint32_t push(std::span<const std::byte> data) noexcept;

    // The bytes of an earlier push into a chunk of this frame.
    [[nodiscard]] std::span<const std::byte> view(std::uint32_t chunk, std::uint32_t offset, std::uint32_t size) const noexcept;

    [[nodiscard]] std::uint32_t getChunk() const noexcept;
    [[nodiscard]] WGPUBuffer getBuffer(std::uint32_t chunk) const noexcept;
    [[nodiscard]] std::size_t getChunkCount() const noexcept;

    // Writes the uniforms of the frame into the buffers of the GPU, before the frame is submitted, and starts the next frame from the first chunk.
    void flush(WGPUQueue queue) noexcept;

  private:
    struct Chunk {
        WGPUBuffer buffer = nullptr;
        std::vector<std::byte> bytes;
        std::size_t used = 0;
    };

    void addChunk();

    WGPUDevice device;
    std::uint32_t alignment;
    std::vector<Chunk> chunks;
    std::uint32_t current = 0;
};

} // namespace haylen::gpu::webgpu
