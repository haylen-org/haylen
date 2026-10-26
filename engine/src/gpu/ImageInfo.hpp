#pragma once

#include <algorithm>
#include <bit>
#include <cstddef>

#include "haylen/gpu/Device.hpp"

namespace haylen::gpu {

// What the pixel formats and the mip levels of images hold, which every device and the resources of the engine read the same way.
class ImageInfo final {
  public:
    // The size of one mip level of an image, whose depth counts the slices of a volume and the layers of the other types.
    struct Extent {
        int width = 0;
        int height = 0;
        int depth = 0;
    };

    [[nodiscard]] static constexpr std::size_t getBytesPerPixel(Device::PixelFormat format) noexcept {
        switch (format) {
        case Device::PixelFormat::R8:
            return 1;
        case Device::PixelFormat::Rg8:
        case Device::PixelFormat::R16Float:
        case Device::PixelFormat::Depth16:
            return 2;
        case Device::PixelFormat::Rgba8:
        case Device::PixelFormat::Rgba8Srgb:
        case Device::PixelFormat::Bgra8:
        case Device::PixelFormat::Rgb10A2:
        case Device::PixelFormat::Rg16Float:
        case Device::PixelFormat::R32Float:
        case Device::PixelFormat::Depth32Float:
        case Device::PixelFormat::Depth24Stencil8:
            return 4;
        case Device::PixelFormat::Rgba16Float:
        case Device::PixelFormat::Rg32Float:
        case Device::PixelFormat::Depth32FloatStencil8:
            return 8;
        case Device::PixelFormat::Rgba32Float:
            return 16;
        }
        return 4;
    }

    [[nodiscard]] static constexpr bool isDepth(Device::PixelFormat format) noexcept {
        return format == Device::PixelFormat::Depth16 || format == Device::PixelFormat::Depth32Float || hasStencil(format);
    }

    [[nodiscard]] static constexpr bool hasStencil(Device::PixelFormat format) noexcept {
        return format == Device::PixelFormat::Depth24Stencil8 || format == Device::PixelFormat::Depth32FloatStencil8;
    }

    // The most mip levels an image of the description can hold, down to one pixel along its largest side.
    [[nodiscard]] static constexpr int getMaxMipLevels(const Device::ImageDesc& desc) noexcept {
        const int depth = desc.type == Device::TextureType::Volume ? desc.layers : 1;
        const int largest = std::max({desc.width, desc.height, depth, 1});
        return std::bit_width(static_cast<unsigned>(largest));
    }

    [[nodiscard]] static constexpr Extent getLevelExtent(const Device::ImageDesc& desc, int level) noexcept {
        const int depth = desc.type == Device::TextureType::Volume ? std::max(1, desc.layers >> level) : desc.layers;
        return {.width = std::max(1, desc.width >> level), .height = std::max(1, desc.height >> level), .depth = depth};
    }

    // The bytes that one mip level holds with every layer or slice of it, tightly packed.
    [[nodiscard]] static constexpr std::size_t getLevelBytes(const Device::ImageDesc& desc, int level) noexcept {
        const Extent extent = getLevelExtent(desc, level);
        return static_cast<std::size_t>(extent.width) * static_cast<std::size_t>(extent.height) * static_cast<std::size_t>(extent.depth) * getBytesPerPixel(desc.format);
    }
};

} // namespace haylen::gpu
