#include "gpu/metal/ImageReadback.hpp"

#include <span>
#include <stdexcept>

#include "gpu/ImageInfo.hpp"
#include "gpu/PixelConverter.hpp"

namespace haylen::gpu::metal {

ImageReadback::ImageReadback(id<MTLCommandQueue> queue, id<MTLTexture> texture, gpu::Device::PixelFormat pixelFormat, int imageWidth, int imageHeight) : format(pixelFormat), width(imageWidth), height(imageHeight) {
    const NSUInteger rowBytes = ImageInfo::getRowBytes(format, width);
    const NSUInteger imageBytes = rowBytes * static_cast<NSUInteger>(height);
    buffer = [queue.device newBufferWithLength:imageBytes options:MTLResourceStorageModeShared];
    if (buffer == nil) {
        throw std::runtime_error("The GPU has no memory to read the pixels of a render target into.");
    }

    @autoreleasepool {
        const id<MTLCommandBuffer> commands = [queue commandBuffer];
        commands.label = @"haylen-readback";
        const id<MTLBlitCommandEncoder> blit = [commands blitCommandEncoder];
        [blit copyFromTexture:texture sourceSlice:0 sourceLevel:0 sourceOrigin:MTLOriginMake(0, 0, 0) sourceSize:MTLSizeMake(static_cast<NSUInteger>(width), static_cast<NSUInteger>(height), 1) toBuffer:buffer destinationOffset:0 destinationBytesPerRow:rowBytes destinationBytesPerImage:imageBytes];
        [blit endEncoding];

        // A copy that failed, such as on a lost device, still ends, so the caller never waits forever.
        const std::shared_ptr<std::atomic<bool>> finished = done;
        [commands addCompletedHandler:^(id<MTLCommandBuffer>) { finished->store(true, std::memory_order_release); }];
        [commands commit];
    }
}

std::optional<std::vector<std::uint8_t>> ImageReadback::poll() {
    if (delivered || !done->load(std::memory_order_acquire)) {
        return std::nullopt;
    }

    delivered = true;
    const std::size_t rowBytes = ImageInfo::getRowBytes(format, width);
    const std::size_t pixelRow = static_cast<std::size_t>(width) * 4U;
    std::vector<std::uint8_t> pixels(pixelRow * static_cast<std::size_t>(height));
    const auto* source = static_cast<const std::byte*>(buffer.contents);
    for (std::size_t row = 0; row < static_cast<std::size_t>(height); ++row) {
        PixelConverter::convertRow(format, {source + row * rowBytes, rowBytes}, std::span(pixels).subspan(row * pixelRow, pixelRow));
    }

    return pixels;
}

} // namespace haylen::gpu::metal
