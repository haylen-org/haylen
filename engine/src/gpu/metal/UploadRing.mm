#include "gpu/metal/UploadRing.hpp"

#include <algorithm>
#include <bit>
#include <cstring>
#include <format>
#include <stdexcept>

namespace haylen::gpu::metal {

UploadRing::UploadRing(id<MTLDevice> metal, std::size_t size) : device(metal) {
    grow(size);
}

void UploadRing::reset() noexcept {
    used = 0;
    outgrown.clear();
}

UploadRing::Allocation UploadRing::write(std::span<const std::byte> data, std::size_t alignment) {
    std::size_t offset = (used + alignment - 1U) & ~(alignment - 1U);
    if (offset + data.size() > capacity) {
        grow(offset + data.size());
        offset = 0;
    }

    std::memcpy(contents + offset, data.data(), data.size());
    used = offset + data.size();
    return {.buffer = buffer, .offset = offset};
}

// The commands that the frame recorded still read the smaller buffer, which stays until the frame starts again. The CPU caches the buffer, since small writes into write-combined memory cost more.
void UploadRing::grow(std::size_t needed) {
    const std::size_t size = std::bit_ceil(std::max(needed, capacity * 2U));
    id<MTLBuffer> grown = [device newBufferWithLength:size options:MTLResourceStorageModeShared];
    if (grown == nil) {
        throw std::runtime_error(std::format("The GPU has no memory for the {} bytes that a frame uploads.", size));
    }
    grown.label = @"haylen-upload-ring";

    if (buffer != nil) {
        outgrown.push_back(buffer);
    }
    buffer = grown;
    contents = static_cast<std::byte*>(buffer.contents);
    capacity = size;
    used = 0;
}

} // namespace haylen::gpu::metal
