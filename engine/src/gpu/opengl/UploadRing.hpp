#pragma once

#include <cstddef>
#include <vector>

#include "gpu/opengl/Gl.hpp"
#include "gpu/opengl/StateCache.hpp"

namespace haylen::gpu::opengl {

// The buffer that carries the new contents of dynamic and stream buffers and the new pixels of dynamic images to them on OpenGL and OpenGL ES, with a region for each frame in flight. The CPU writes the region of a frame once the fence of the frame that used it last passed, so it maps it without waiting for the GPU, persistently where the context has buffer storage, and the GPU copies from it.
class UploadRing final {
  public:
    static constexpr std::size_t kInitialRegionSize = std::size_t{1} << 20U;
    static constexpr std::size_t kAlignment = 16;

    // Memory of the ring that the CPU writes, at an offset of the buffer of the ring.
    struct Slice {
        std::byte* data = nullptr;
        std::size_t offset = 0;
    };

    [[nodiscard]] static constexpr std::size_t align(std::size_t size) noexcept {
        return (size + kAlignment - 1) / kAlignment * kAlignment;
    }

    UploadRing(Gl& functions, StateCache& cache, std::size_t regions);
    UploadRing(const UploadRing&) = delete;
    UploadRing& operator=(const UploadRing&) = delete;
    ~UploadRing();

    // Starts writing the region of the frame from its beginning.
    void beginFrame(std::size_t frame) noexcept;

    // Maps the bytes at the end of what the frame wrote, growing the ring when its region is full. The GPU reads them only after `unmap`.
    [[nodiscard]] Slice map(std::size_t size);
    void unmap();

    [[nodiscard]] Gl::Uint getBuffer() const noexcept {
        return buffer;
    }

    [[nodiscard]] std::size_t getSize() const noexcept {
        return regionSize * regionCount;
    }

    // Makes the ring as small as the last frame of each region needed, and never smaller than its first size. OpenGL keeps the buffer that the ring lets go of until the copies the GPU has yet to make from it finished.
    void trim();

    // The bytes of the regions beyond what the last frame of each region needed.
    [[nodiscard]] std::size_t getSpareBytes() const noexcept;

  private:
    void allocate(std::size_t size);
    void release() noexcept;
    [[nodiscard]] std::size_t getNeededRegionSize() const noexcept;

    Gl& gl;
    StateCache& state;
    std::size_t regionCount = 0;
    std::size_t regionSize = 0;
    std::size_t region = 0;
    std::size_t cursor = 0;
    // The bytes that the last frame of each region wrote.
    std::vector<std::size_t> used;
    Gl::Uint buffer = 0;
    std::byte* persistentMemory = nullptr;
};

} // namespace haylen::gpu::opengl
