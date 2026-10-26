#pragma once

#include <cstddef>

#include "gpu/opengl/Gl.hpp"
#include "gpu/opengl/StateCache.hpp"

namespace haylen::gpu::opengl {

// The buffer that carries the new contents of dynamic buffers and the new pixels of dynamic images to them on OpenGL and OpenGL ES, with a region for each frame in flight. The CPU writes the region of a frame once the fence of the frame that used it last passed, so it maps it without waiting for the GPU, persistently where the context has buffer storage, and the GPU copies from it.
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

  private:
    void allocate(std::size_t size);
    void release() noexcept;

    Gl& gl;
    StateCache& state;
    std::size_t regionCount = 0;
    std::size_t regionSize = 0;
    std::size_t region = 0;
    std::size_t cursor = 0;
    Gl::Uint buffer = 0;
    std::byte* persistentMemory = nullptr;
};

} // namespace haylen::gpu::opengl
