#include "gpu/opengl/UploadRing.hpp"

#include <algorithm>
#include <stdexcept>

namespace haylen::gpu::opengl {

UploadRing::UploadRing(Gl& functions, StateCache& cache, std::size_t regions) : gl(functions), state(cache), regionCount(regions) {
    allocate(kInitialRegionSize);
}

UploadRing::~UploadRing() {
    release();
}

void UploadRing::allocate(std::size_t size) {
    regionSize = size;
    cursor = region * regionSize;
    gl.genBuffers(1, &buffer);
    state.bindBuffer(Gl::kCopyReadBuffer, buffer);

    const auto total = static_cast<Gl::SizeiPtr>(getSize());
    if (!gl.getFeatures().bufferStorage) {
        gl.bufferData(Gl::kCopyReadBuffer, total, nullptr, Gl::kStreamDraw);
        return;
    }
    const Gl::Bitfield flags = Gl::kMapWriteBit | Gl::kMapPersistentBit | Gl::kMapCoherentBit;
    gl.bufferStorage(Gl::kCopyReadBuffer, total, nullptr, flags);
    persistentMemory = static_cast<std::byte*>(gl.mapBufferRange(Gl::kCopyReadBuffer, 0, total, flags));
    if (persistentMemory == nullptr) {
        throw std::runtime_error("The OpenGL context could not map the buffer that uploads data to the GPU.");
    }
}

// Deleting the buffer unmaps it, and OpenGL keeps it until the copies the GPU has yet to make from it finished.
void UploadRing::release() noexcept {
    if (buffer == 0) {
        return;
    }
    gl.deleteBuffers(1, &buffer);
    state.forgetBuffer(buffer);
    buffer = 0;
    persistentMemory = nullptr;
}

void UploadRing::beginFrame(std::size_t frame) noexcept {
    region = frame % regionCount;
    cursor = region * regionSize;
}

UploadRing::Slice UploadRing::map(std::size_t size) {
    const std::size_t start = (cursor + kAlignment - 1) / kAlignment * kAlignment;
    if (start + size > (region + 1) * regionSize) {
        release();
        allocate(std::max(regionSize * 2, (size + kAlignment - 1) / kAlignment * kAlignment));
        return map(size);
    }
    cursor = start + size;

    if (persistentMemory != nullptr) {
        return {.data = persistentMemory + start, .offset = start};
    }
    state.bindBuffer(Gl::kCopyReadBuffer, buffer);
    const Gl::Bitfield access = Gl::kMapWriteBit | Gl::kMapUnsynchronizedBit | Gl::kMapInvalidateRangeBit;
    auto* data = static_cast<std::byte*>(gl.mapBufferRange(Gl::kCopyReadBuffer, static_cast<Gl::IntPtr>(start), static_cast<Gl::SizeiPtr>(size), access));
    if (data == nullptr) {
        throw std::runtime_error("The OpenGL context could not map the buffer that uploads data to the GPU.");
    }
    return {.data = data, .offset = start};
}

void UploadRing::unmap() {
    if (persistentMemory != nullptr) {
        return;
    }
    state.bindBuffer(Gl::kCopyReadBuffer, buffer);
    gl.unmapBuffer(Gl::kCopyReadBuffer);
}

} // namespace haylen::gpu::opengl
