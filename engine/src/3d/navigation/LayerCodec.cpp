#include "3d/navigation/LayerCodec.hpp"

#include <zstd.h>

#include <stdexcept>

namespace haylen::navigation3d {

std::vector<std::uint8_t> LayerCodec::compress(std::span<const std::uint8_t> heights, std::span<const std::uint8_t> areas, std::span<const std::uint8_t> connections) {
    std::vector<std::uint8_t> voxels;
    voxels.reserve(heights.size() + areas.size() + connections.size());
    voxels.insert(voxels.end(), heights.begin(), heights.end());
    voxels.insert(voxels.end(), areas.begin(), areas.end());
    voxels.insert(voxels.end(), connections.begin(), connections.end());

    std::vector<std::uint8_t> compressed(ZSTD_compressBound(voxels.size()));
    const std::size_t size = ZSTD_compress(compressed.data(), compressed.size(), voxels.data(), voxels.size(), kLevel);
    if (ZSTD_isError(size) != 0U) {
        throw std::runtime_error("A layer of a navigation mesh did not compress, since the system ran out of memory.");
    }

    compressed.resize(size);
    compressed.shrink_to_fit();
    return compressed;
}

bool LayerCodec::decompress(std::span<const std::uint8_t> compressed, std::size_t size, std::vector<std::uint8_t>& voxels) {
    if (ZSTD_getFrameContentSize(compressed.data(), compressed.size()) != size) {
        return false;
    }

    voxels.resize(size);
    const std::size_t written = ZSTD_decompress(voxels.data(), voxels.size(), compressed.data(), compressed.size());
    return ZSTD_isError(written) == 0U && written == size;
}

} // namespace haylen::navigation3d
