#include "gpu/metal/ImageReadback.hpp"

#include <cstddef>
#include <span>

#include "gpu/ImageInfo.hpp"
#include "gpu/PixelConverter.hpp"

namespace haylen::gpu::metal {

ImageReadback::ImageReadback(id<MTLCommandQueue> queue, id<MTLTexture> texture, gpu::Device::PixelFormat pixelFormat, int imageWidth, int imageHeight) : copy(queue, ImageInfo::getRowBytes(pixelFormat, imageWidth) * static_cast<std::size_t>(imageHeight)), format(pixelFormat), width(imageWidth), height(imageHeight) {
    const NSUInteger rowBytes = ImageInfo::getRowBytes(format, width);
    const NSUInteger imageBytes = rowBytes * static_cast<NSUInteger>(height);
    const MTLSize size = MTLSizeMake(static_cast<NSUInteger>(width), static_cast<NSUInteger>(height), 1);
    [copy.getBlit() copyFromTexture:texture sourceSlice:0 sourceLevel:0 sourceOrigin:MTLOriginMake(0, 0, 0) sourceSize:size toBuffer:copy.getBuffer() destinationOffset:0 destinationBytesPerRow:rowBytes destinationBytesPerImage:imageBytes];
    copy.submit();
}

std::optional<std::vector<std::uint8_t>> ImageReadback::poll() {
    const std::optional<std::span<const std::byte>> bytes = copy.take();
    if (!bytes) {
        return std::nullopt;
    }

    const std::size_t rowBytes = ImageInfo::getRowBytes(format, width);
    const std::size_t pixelRow = static_cast<std::size_t>(width) * 4U;
    std::vector<std::uint8_t> pixels(pixelRow * static_cast<std::size_t>(height));
    for (std::size_t row = 0; row < static_cast<std::size_t>(height); ++row) {
        PixelConverter::convertRow(format, bytes->subspan(row * rowBytes, rowBytes), std::span(pixels).subspan(row * pixelRow, pixelRow));
    }

    return pixels;
}

} // namespace haylen::gpu::metal
