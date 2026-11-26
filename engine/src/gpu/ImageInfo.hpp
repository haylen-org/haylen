#pragma once

#include <algorithm>
#include <bit>
#include <cstddef>
#include <cstdint>

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

    // The pixels that one block of a format holds and the bytes it takes. An uncompressed format holds one pixel in a block, and a block compressed format a rectangle of pixels that its bytes encode together.
    struct Block {
        int width = 1;
        int height = 1;
        std::size_t bytes = 0;
    };

    [[nodiscard]] static constexpr Block getBlock(Device::PixelFormat format) noexcept {
        switch (format) {
        case Device::PixelFormat::R8:
            return {.width = 1, .height = 1, .bytes = 1};
        case Device::PixelFormat::Rg8:
        case Device::PixelFormat::R16Float:
        case Device::PixelFormat::Depth16:
            return {.width = 1, .height = 1, .bytes = 2};
        case Device::PixelFormat::Rgba8:
        case Device::PixelFormat::Rgba8Srgb:
        case Device::PixelFormat::Bgra8:
        case Device::PixelFormat::Rgb10A2:
        case Device::PixelFormat::Rg11B10Float:
        case Device::PixelFormat::Rg16Float:
        case Device::PixelFormat::R32Float:
        case Device::PixelFormat::Depth32Float:
        case Device::PixelFormat::Depth24Stencil8:
            return {.width = 1, .height = 1, .bytes = 4};
        case Device::PixelFormat::Rgba16Float:
        case Device::PixelFormat::Rg32Float:
        case Device::PixelFormat::Depth32FloatStencil8:
            return {.width = 1, .height = 1, .bytes = 8};
        case Device::PixelFormat::Rgba32Float:
            return {.width = 1, .height = 1, .bytes = 16};
        case Device::PixelFormat::Bc1:
        case Device::PixelFormat::Bc1Srgb:
        case Device::PixelFormat::Bc4:
        case Device::PixelFormat::Bc4Snorm:
        case Device::PixelFormat::Etc2Rgb8:
        case Device::PixelFormat::Etc2Rgb8Srgb:
        case Device::PixelFormat::Etc2Rgb8A1:
        case Device::PixelFormat::Etc2Rgb8A1Srgb:
        case Device::PixelFormat::EacR11:
        case Device::PixelFormat::EacR11Snorm:
            return {.width = 4, .height = 4, .bytes = 8};
        case Device::PixelFormat::Bc2:
        case Device::PixelFormat::Bc2Srgb:
        case Device::PixelFormat::Bc3:
        case Device::PixelFormat::Bc3Srgb:
        case Device::PixelFormat::Bc5:
        case Device::PixelFormat::Bc5Snorm:
        case Device::PixelFormat::Bc6hUfloat:
        case Device::PixelFormat::Bc6hFloat:
        case Device::PixelFormat::Bc7:
        case Device::PixelFormat::Bc7Srgb:
        case Device::PixelFormat::Etc2Rgba8:
        case Device::PixelFormat::Etc2Rgba8Srgb:
        case Device::PixelFormat::EacRg11:
        case Device::PixelFormat::EacRg11Snorm:
        case Device::PixelFormat::Astc4x4:
        case Device::PixelFormat::Astc4x4Srgb:
        case Device::PixelFormat::Astc4x4Hdr:
            return {.width = 4, .height = 4, .bytes = 16};
        case Device::PixelFormat::Astc5x4:
        case Device::PixelFormat::Astc5x4Srgb:
        case Device::PixelFormat::Astc5x4Hdr:
            return {.width = 5, .height = 4, .bytes = 16};
        case Device::PixelFormat::Astc5x5:
        case Device::PixelFormat::Astc5x5Srgb:
        case Device::PixelFormat::Astc5x5Hdr:
            return {.width = 5, .height = 5, .bytes = 16};
        case Device::PixelFormat::Astc6x5:
        case Device::PixelFormat::Astc6x5Srgb:
        case Device::PixelFormat::Astc6x5Hdr:
            return {.width = 6, .height = 5, .bytes = 16};
        case Device::PixelFormat::Astc6x6:
        case Device::PixelFormat::Astc6x6Srgb:
        case Device::PixelFormat::Astc6x6Hdr:
            return {.width = 6, .height = 6, .bytes = 16};
        case Device::PixelFormat::Astc8x5:
        case Device::PixelFormat::Astc8x5Srgb:
        case Device::PixelFormat::Astc8x5Hdr:
            return {.width = 8, .height = 5, .bytes = 16};
        case Device::PixelFormat::Astc8x6:
        case Device::PixelFormat::Astc8x6Srgb:
        case Device::PixelFormat::Astc8x6Hdr:
            return {.width = 8, .height = 6, .bytes = 16};
        case Device::PixelFormat::Astc8x8:
        case Device::PixelFormat::Astc8x8Srgb:
        case Device::PixelFormat::Astc8x8Hdr:
            return {.width = 8, .height = 8, .bytes = 16};
        case Device::PixelFormat::Astc10x5:
        case Device::PixelFormat::Astc10x5Srgb:
        case Device::PixelFormat::Astc10x5Hdr:
            return {.width = 10, .height = 5, .bytes = 16};
        case Device::PixelFormat::Astc10x6:
        case Device::PixelFormat::Astc10x6Srgb:
        case Device::PixelFormat::Astc10x6Hdr:
            return {.width = 10, .height = 6, .bytes = 16};
        case Device::PixelFormat::Astc10x8:
        case Device::PixelFormat::Astc10x8Srgb:
        case Device::PixelFormat::Astc10x8Hdr:
            return {.width = 10, .height = 8, .bytes = 16};
        case Device::PixelFormat::Astc10x10:
        case Device::PixelFormat::Astc10x10Srgb:
        case Device::PixelFormat::Astc10x10Hdr:
            return {.width = 10, .height = 10, .bytes = 16};
        case Device::PixelFormat::Astc12x10:
        case Device::PixelFormat::Astc12x10Srgb:
        case Device::PixelFormat::Astc12x10Hdr:
            return {.width = 12, .height = 10, .bytes = 16};
        case Device::PixelFormat::Astc12x12:
        case Device::PixelFormat::Astc12x12Srgb:
        case Device::PixelFormat::Astc12x12Hdr:
            return {.width = 12, .height = 12, .bytes = 16};
        }
        return {.width = 1, .height = 1, .bytes = 4};
    }

    // The families of block compressed formats, which graphics APIs and GPUs take as a whole: BC, ETC2 with EAC, and ASTC with the formats of its HDR profile apart.
    enum class Compression : std::uint8_t {
        None,
        Bc,
        Etc2,
        Astc,
        AstcHdr,
    };

    [[nodiscard]] static constexpr Compression getCompression(Device::PixelFormat format) noexcept {
        switch (format) {
        case Device::PixelFormat::Bc1:
        case Device::PixelFormat::Bc1Srgb:
        case Device::PixelFormat::Bc2:
        case Device::PixelFormat::Bc2Srgb:
        case Device::PixelFormat::Bc3:
        case Device::PixelFormat::Bc3Srgb:
        case Device::PixelFormat::Bc4:
        case Device::PixelFormat::Bc4Snorm:
        case Device::PixelFormat::Bc5:
        case Device::PixelFormat::Bc5Snorm:
        case Device::PixelFormat::Bc6hUfloat:
        case Device::PixelFormat::Bc6hFloat:
        case Device::PixelFormat::Bc7:
        case Device::PixelFormat::Bc7Srgb:
            return Compression::Bc;
        case Device::PixelFormat::Etc2Rgb8:
        case Device::PixelFormat::Etc2Rgb8Srgb:
        case Device::PixelFormat::Etc2Rgb8A1:
        case Device::PixelFormat::Etc2Rgb8A1Srgb:
        case Device::PixelFormat::Etc2Rgba8:
        case Device::PixelFormat::Etc2Rgba8Srgb:
        case Device::PixelFormat::EacR11:
        case Device::PixelFormat::EacR11Snorm:
        case Device::PixelFormat::EacRg11:
        case Device::PixelFormat::EacRg11Snorm:
            return Compression::Etc2;
        case Device::PixelFormat::Astc4x4:
        case Device::PixelFormat::Astc4x4Srgb:
        case Device::PixelFormat::Astc5x4:
        case Device::PixelFormat::Astc5x4Srgb:
        case Device::PixelFormat::Astc5x5:
        case Device::PixelFormat::Astc5x5Srgb:
        case Device::PixelFormat::Astc6x5:
        case Device::PixelFormat::Astc6x5Srgb:
        case Device::PixelFormat::Astc6x6:
        case Device::PixelFormat::Astc6x6Srgb:
        case Device::PixelFormat::Astc8x5:
        case Device::PixelFormat::Astc8x5Srgb:
        case Device::PixelFormat::Astc8x6:
        case Device::PixelFormat::Astc8x6Srgb:
        case Device::PixelFormat::Astc8x8:
        case Device::PixelFormat::Astc8x8Srgb:
        case Device::PixelFormat::Astc10x5:
        case Device::PixelFormat::Astc10x5Srgb:
        case Device::PixelFormat::Astc10x6:
        case Device::PixelFormat::Astc10x6Srgb:
        case Device::PixelFormat::Astc10x8:
        case Device::PixelFormat::Astc10x8Srgb:
        case Device::PixelFormat::Astc10x10:
        case Device::PixelFormat::Astc10x10Srgb:
        case Device::PixelFormat::Astc12x10:
        case Device::PixelFormat::Astc12x10Srgb:
        case Device::PixelFormat::Astc12x12:
        case Device::PixelFormat::Astc12x12Srgb:
            return Compression::Astc;
        case Device::PixelFormat::Astc4x4Hdr:
        case Device::PixelFormat::Astc5x4Hdr:
        case Device::PixelFormat::Astc5x5Hdr:
        case Device::PixelFormat::Astc6x5Hdr:
        case Device::PixelFormat::Astc6x6Hdr:
        case Device::PixelFormat::Astc8x5Hdr:
        case Device::PixelFormat::Astc8x6Hdr:
        case Device::PixelFormat::Astc8x8Hdr:
        case Device::PixelFormat::Astc10x5Hdr:
        case Device::PixelFormat::Astc10x6Hdr:
        case Device::PixelFormat::Astc10x8Hdr:
        case Device::PixelFormat::Astc10x10Hdr:
        case Device::PixelFormat::Astc12x10Hdr:
        case Device::PixelFormat::Astc12x12Hdr:
            return Compression::AstcHdr;
        default:
            return Compression::None;
        }
    }

    [[nodiscard]] static constexpr bool isCompressed(Device::PixelFormat format) noexcept {
        return getBlock(format).width > 1;
    }

    // The bytes of one row of blocks of pixels of the width, whose last block may hold columns beyond it.
    [[nodiscard]] static constexpr std::size_t getRowBytes(Device::PixelFormat format, int width) noexcept {
        const Block block = getBlock(format);
        return static_cast<std::size_t>((width + block.width - 1) / block.width) * block.bytes;
    }

    // The rows of blocks that hold pixels of the height, whose last row may hold rows beyond it.
    [[nodiscard]] static constexpr int getRowCount(Device::PixelFormat format, int height) noexcept {
        const Block block = getBlock(format);
        return (height + block.height - 1) / block.height;
    }

    // The bytes of a rectangle of pixels of the format, tightly packed in rows of blocks.
    [[nodiscard]] static constexpr std::size_t getRegionBytes(Device::PixelFormat format, int width, int height) noexcept {
        return getRowBytes(format, width) * static_cast<std::size_t>(getRowCount(format, height));
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

    // The bytes that one mip level holds with every layer or slice of it, tightly packed in rows of blocks.
    [[nodiscard]] static constexpr std::size_t getLevelBytes(const Device::ImageDesc& desc, int level) noexcept {
        const Extent extent = getLevelExtent(desc, level);
        return getRegionBytes(desc.format, extent.width, extent.height) * static_cast<std::size_t>(extent.depth);
    }
};

} // namespace haylen::gpu
