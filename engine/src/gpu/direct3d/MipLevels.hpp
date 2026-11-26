#pragma once

#include <cstddef>

#include "haylen/gpu/Device.hpp"

namespace haylen::gpu::direct3d {

// The sizes of the mip levels of an image, as Direct3D lays them out, in pixels and in rows of blocks of pixels, which are rows of pixels for an uncompressed format. Arrays and cubes keep their layers at every level, while the slices of a volume halve with its width and height.
class MipLevels final {
  public:
    struct Level {
        std::size_t width = 0;
        std::size_t height = 0;
        std::size_t layers = 0;
        std::size_t rows = 0;
        std::size_t rowBytes = 0;
        std::size_t sliceBytes = 0;
    };

    [[nodiscard]] static Level describe(Device::TextureType type, int width, int height, int layers, Device::PixelFormat format, int level) noexcept;
};

} // namespace haylen::gpu::direct3d
