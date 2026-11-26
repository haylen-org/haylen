#pragma once

#include <cstddef>
#include <cstdint>

namespace haylen::gltf {

// The most that one glTF file may ask the reader to hold, so a hostile file never exhausts memory or time.
struct Limits {
    std::uint64_t maxJsonBytes = std::uint64_t{64} << 20U;
    std::size_t maxJsonDepth = 128;

    // The bytes of every buffer together, and of the values that one accessor gives when it is read.
    std::uint64_t maxBufferBytes = std::uint64_t{1} << 30U;
    std::uint64_t maxDataUriBytes = std::uint64_t{32} << 20U;
    std::uint64_t maxImageBytes = std::uint64_t{256} << 20U;

    // The objects of every list of the document together.
    std::size_t maxObjects = std::size_t{1} << 21U;
};

} // namespace haylen::gltf
