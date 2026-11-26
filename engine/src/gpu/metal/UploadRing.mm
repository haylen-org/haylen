#include "gpu/metal/UploadRing.hpp"

#include <algorithm>
#include <bit>
#include <cstring>
#include <format>
#include <stdexcept>

namespace haylen::gpu::metal {

UploadRing::UploadRing(id<MTLDevice> metal, std::size_t size) : device(metal), initialCapacity(size) {
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

void UploadRing::trim() {
    outgrown.clear();
    const std::size_t needed = getNeededCapacity();
    if (needed < capacity) {
        replace(needed);
    }
}

std::size_t UploadRing::getSpareBytes() const noexcept {
    std::size_t spare = capacity - std::min(capacity, getNeededCapacity());
    for (id<MTLBuffer> old : outgrown) {
        spare += old.length;
    }

    return spare;
}

std::size_t UploadRing::getNeededCapacity() const noexcept {
    return std::max(initialCapacity, std::bit_ceil(used));
}

// The commands that the frame recorded still read the smaller buffer, which stays until the frame starts again.
void UploadRing::grow(std::size_t needed) {
    if (buffer != nil) {
        outgrown.push_back(buffer);
    }

    replace(std::bit_ceil(std::max(needed, capacity * 2U)));
    used = 0;
}

// The CPU caches the buffer, since small writes into write-combined memory cost more.
void UploadRing::replace(std::size_t size) {
    id<MTLBuffer> grown = [device newBufferWithLength:size options:MTLResourceStorageModeShared];
    if (grown == nil) {
        throw std::runtime_error(std::format("The GPU has no memory for the {} bytes that a frame uploads.", size));
    }

    grown.label = @"haylen-upload-ring";
    buffer = grown;
    contents = static_cast<std::byte*>(buffer.contents);
    capacity = size;
}

} // namespace haylen::gpu::metal
