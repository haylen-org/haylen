#pragma once

#include <array>
#include <cstddef>
#include <span>
#include <vector>

#include "gpu/opengl/Gl.hpp"
#include "gpu/opengl/StateCache.hpp"
#include "haylen/gpu/Device.hpp"

namespace haylen::gpu::opengl {

// The bytes that `applyUniforms` gives the uniform blocks of a frame, which the draws read as ranges of the buffers of the ring at offsets of the alignment of the context. Each frame in flight writes chunks of its own, which a later frame writes again once the fence of the frame that wrote them last passed, through a persistent mapping where the context has buffer storage, through a mapping without synchronization on the other contexts of OpenGL and OpenGL ES, and with `glBufferSubData` on WebGL2, whose browser stages the bytes. A chunk keeps its buffer while its frame runs, so a frame that needs more room moves on to another chunk and the ranges it bound before keep their buffer. A slot whose bytes come again in the frame takes the range they went to, so a frame that repeats a block sends it once.
class UniformRing final {
  public:
    static constexpr std::size_t kChunkSize = std::size_t{1} << 20U;

    UniformRing(Gl& functions, StateCache& cache, std::size_t regionCount, std::size_t offsetAlignment);
    UniformRing(const UniformRing&) = delete;
    UniformRing& operator=(const UniformRing&) = delete;
    ~UniformRing();

    // Starts writing the chunks of the region of the frame from the first one.
    void beginFrame(std::size_t frame) noexcept;

    // Writes the bytes of the block at the slot, unless the slot took the same bytes since the frame began, and returns the range that holds them.
    [[nodiscard]] StateCache::UniformRange write(std::size_t slot, std::span<const std::byte> data);

    // Releases the chunks of each region beyond those that its last frame wrote. OpenGL keeps a buffer that the ring deletes until the draws that read it finished.
    void trim();

    // The bytes of the chunks of each region beyond those that its last frame wrote.
    [[nodiscard]] std::size_t getSpareBytes() const noexcept;

  private:
    // A buffer of the ring, with the memory that the CPU writes it through where the context maps it persistently.
    struct Chunk {
        Gl::Uint buffer = 0;
        std::size_t size = 0;
        std::byte* memory = nullptr;
    };

    // The chunks of a frame in flight, and how many of them its last frame wrote.
    struct Region {
        std::vector<Chunk> chunks;
        std::size_t used = 0;
    };

    // The bytes that a slot took last since the frame began and the range that holds them.
    struct Upload {
        std::vector<std::byte> bytes;
        StateCache::UniformRange range;
    };

    [[nodiscard]] Chunk makeChunk(std::size_t size);
    void releaseChunk(Chunk& chunk) noexcept;
    void advance(std::size_t size);
    void copy(const Chunk& chunk, std::size_t offset, std::span<const std::byte> data);

    Gl& gl;
    StateCache& state;
    std::size_t alignment = 1;
    std::vector<Region> regions;
    std::size_t region = 0;
    std::size_t cursor = 0;
    std::array<Upload, Device::kMaxBlocks> uploads{};
};

} // namespace haylen::gpu::opengl
