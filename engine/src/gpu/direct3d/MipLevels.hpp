#pragma once

#include <cstddef>

#include "haylen/gpu/Device.hpp"

namespace haylen::gpu::direct3d {

// The sizes of the mip levels of an image and the check that the pixels of a texture fill each of them. Arrays and cubes keep their layers at every level, while the slices of a volume halve with its width and height.
class MipLevels final {
  public:
    struct Level {
        std::size_t width = 0;
        std::size_t height = 0;
        std::size_t layers = 0;
        std::size_t rowBytes = 0;
        std::size_t sliceBytes = 0;
    };

    [[nodiscard]] static Level describe(Device::TextureType type, int width, int height, int layers, Device::PixelFormat format, int level) noexcept;

    // Throws `std::logic_error` when the pixels of the texture do not hold one span for each mip level, each with every layer of its level.
    static void check(const Device::ImageDesc& desc);
};

} // namespace haylen::gpu::direct3d
