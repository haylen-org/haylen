#include "graphics/GpuReadback.hpp"

#import <Metal/Metal.h>

#include <atomic>
#include <cstring>
#include <stdexcept>

namespace haylen::graphics {

// The blit runs on its own command buffer of the queue of sokol_gfx, after the command buffer of the frame, and its completion handler marks the copy done.
struct GpuReadback::Pending {
    id<MTLBuffer> buffer = nil;
    std::shared_ptr<std::atomic<bool>> done = std::make_shared<std::atomic<bool>>(false);
};

GpuReadback::GpuReadback(sg_image image, int imageWidth, int imageHeight) : width(imageWidth), height(imageHeight) {
    if (sg_query_backend() != SG_BACKEND_METAL_MACOS && sg_query_backend() != SG_BACKEND_METAL_IOS && sg_query_backend() != SG_BACKEND_METAL_SIMULATOR) {
        return;
    }
    const sg_mtl_image_info info = sg_mtl_query_image_info(image);
    const id<MTLTexture> texture = (__bridge id<MTLTexture>)info.tex[info.active_slot];
    const id<MTLDevice> device = (__bridge id<MTLDevice>)sg_mtl_device();
    const id<MTLCommandQueue> queue = (__bridge id<MTLCommandQueue>)sg_mtl_command_queue();
    const auto rowBytes = static_cast<NSUInteger>(width) * 4U;
    pending = std::make_unique<Pending>();
    pending->buffer = [device newBufferWithLength:rowBytes * static_cast<NSUInteger>(height) options:MTLResourceStorageModeShared];
    if (pending->buffer == nil) {
        throw std::runtime_error("The GPU found no memory to read the pixels of a render target into.");
    }
    const id<MTLCommandBuffer> commands = [queue commandBuffer];
    const id<MTLBlitCommandEncoder> blit = [commands blitCommandEncoder];
    [blit copyFromTexture:texture sourceSlice:0 sourceLevel:0 sourceOrigin:MTLOriginMake(0, 0, 0) sourceSize:MTLSizeMake(static_cast<NSUInteger>(width), static_cast<NSUInteger>(height), 1) toBuffer:pending->buffer destinationOffset:0 destinationBytesPerRow:rowBytes destinationBytesPerImage:rowBytes * static_cast<NSUInteger>(height)];
    [blit endEncoding];
    const std::shared_ptr<std::atomic<bool>> done = pending->done;
    [commands addCompletedHandler:^(id<MTLCommandBuffer>) { done->store(true); }];
    [commands commit];
}

GpuReadback::~GpuReadback() = default;

std::optional<std::vector<std::uint8_t>> GpuReadback::poll() {
    const auto size = static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4U;
    if (!pending) {
        return std::vector<std::uint8_t>(size, 0);
    }
    if (!pending->done->load()) {
        return std::nullopt;
    }
    std::vector<std::uint8_t> pixels(size);
    std::memcpy(pixels.data(), pending->buffer.contents, size);
    return pixels;
}

} // namespace haylen::graphics
