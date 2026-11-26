#include "gpu/opengl/UniformRing.hpp"

#include <algorithm>
#include <cstring>
#include <stdexcept>

namespace haylen::gpu::opengl {

UniformRing::UniformRing(Gl& functions, StateCache& cache, std::size_t regionCount, std::size_t offsetAlignment) : gl(functions), state(cache), alignment(offsetAlignment), regions(regionCount) {}

UniformRing::~UniformRing() {
    for (Region& kept : regions) {
        for (Chunk& chunk : kept.chunks) {
            releaseChunk(chunk);
        }
    }
}

void UniformRing::beginFrame(std::size_t frame) noexcept {
    region = frame % regions.size();
    regions[region].used = 0;
    cursor = 0;

    for (Upload& upload : uploads) {
        upload.bytes.clear();
    }
}

StateCache::UniformRange UniformRing::write(std::size_t slot, std::span<const std::byte> data) {
    Upload& upload = uploads[slot];
    if (!upload.bytes.empty() && std::ranges::equal(upload.bytes, data)) {
        return upload.range;
    }

    const Region& current = regions[region];
    std::size_t start = (cursor + alignment - 1) / alignment * alignment;
    if (current.used == 0 || start + data.size() > current.chunks[current.used - 1].size) {
        advance(data.size());
        start = 0;
    }

    const Chunk& chunk = current.chunks[current.used - 1];
    copy(chunk, start, data);
    cursor = start + data.size();

    upload.bytes.assign(data.begin(), data.end());
    upload.range = {.buffer = chunk.buffer, .offset = static_cast<Gl::IntPtr>(start), .size = static_cast<Gl::SizeiPtr>(data.size())};
    return upload.range;
}

// The frame moves on to its next chunk, which an earlier frame of the region made, or which it makes when the region has no more or when the chunk is too small for the bytes. No range of the frame names that chunk yet, so it may be replaced.
void UniformRing::advance(std::size_t size) {
    Region& current = regions[region];
    if (current.used == current.chunks.size()) {
        current.chunks.push_back(makeChunk(std::max(kChunkSize, size)));
    } else if (current.chunks[current.used].size < size) {
        releaseChunk(current.chunks[current.used]);
        current.chunks[current.used] = makeChunk(size);
    }

    ++current.used;
}

UniformRing::Chunk UniformRing::makeChunk(std::size_t size) {
    Chunk chunk{.size = size};
    gl.genBuffers(1, &chunk.buffer);
    state.bindBuffer(Gl::kUniformBuffer, chunk.buffer);

    const auto total = static_cast<Gl::SizeiPtr>(size);
    if (!gl.getFeatures().bufferStorage) {
        gl.bufferData(Gl::kUniformBuffer, total, nullptr, Gl::kStreamDraw);
        return chunk;
    }

    const Gl::Bitfield flags = Gl::kMapWriteBit | Gl::kMapPersistentBit | Gl::kMapCoherentBit;
    gl.bufferStorage(Gl::kUniformBuffer, total, nullptr, flags);
    chunk.memory = static_cast<std::byte*>(gl.mapBufferRange(Gl::kUniformBuffer, 0, total, flags));
    if (chunk.memory == nullptr) {
        releaseChunk(chunk);
        throw std::runtime_error("The OpenGL context could not map the buffer that carries the uniforms of the draws to the GPU.");
    }

    return chunk;
}

// Deleting the buffer unmaps it and unbinds it from every binding point of the context.
void UniformRing::releaseChunk(Chunk& chunk) noexcept {
    if (chunk.buffer == 0) {
        return;
    }

    gl.deleteBuffers(1, &chunk.buffer);
    state.forgetBuffer(chunk.buffer);
    chunk = {};
}

// The fence of the frame that wrote the region last passed before the region began again, so a mapping without synchronization overwrites nothing that the GPU still reads.
void UniformRing::copy(const Chunk& chunk, std::size_t offset, std::span<const std::byte> data) {
    if (chunk.memory != nullptr) {
        std::memcpy(chunk.memory + offset, data.data(), data.size());
        return;
    }

    const auto start = static_cast<Gl::IntPtr>(offset);
    const auto size = static_cast<Gl::SizeiPtr>(data.size());
    state.bindBuffer(Gl::kUniformBuffer, chunk.buffer);
    if (gl.isWeb()) {
        gl.bufferSubData(Gl::kUniformBuffer, start, size, data.data());
        return;
    }

    const Gl::Bitfield access = Gl::kMapWriteBit | Gl::kMapUnsynchronizedBit | Gl::kMapInvalidateRangeBit;
    void* mapped = gl.mapBufferRange(Gl::kUniformBuffer, start, size, access);
    if (mapped == nullptr) {
        throw std::runtime_error("The OpenGL context could not map the buffer that carries the uniforms of the draws to the GPU.");
    }

    std::memcpy(mapped, data.data(), data.size());
    gl.unmapBuffer(Gl::kUniformBuffer);
}

void UniformRing::trim() {
    for (Region& kept : regions) {
        for (std::size_t index = kept.used; index < kept.chunks.size(); ++index) {
            releaseChunk(kept.chunks[index]);
        }

        kept.chunks.resize(kept.used);
    }
}

std::size_t UniformRing::getSpareBytes() const noexcept {
    std::size_t spare = 0;
    for (const Region& kept : regions) {
        for (std::size_t index = kept.used; index < kept.chunks.size(); ++index) {
            spare += kept.chunks[index].size;
        }
    }

    return spare;
}

} // namespace haylen::gpu::opengl
