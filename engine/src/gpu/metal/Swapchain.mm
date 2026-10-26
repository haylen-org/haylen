#include "gpu/metal/Swapchain.hpp"

#include <format>
#include <stdexcept>

namespace haylen::gpu::metal {

Swapchain::Swapchain(id<MTLDevice> metal, Graveyard& retired, int samples, bool tileMemory) : device(metal), graveyard(retired), sampleCount(samples), memoryless(tileMemory) {}

void Swapchain::attach(const Surface& surface) {
    if (surface.kind != Surface::Kind::MetalLayer || surface.layer == nullptr) {
        throw std::runtime_error("The Metal device draws only into a surface that holds a \"CAMetalLayer\".");
    }
    if (surface.sampleCount != sampleCount) {
        throw std::runtime_error(std::format("The surface asks for {} samples, and the swapchain of this device draws with {}.", surface.sampleCount, sampleCount));
    }

    layer = (__bridge CAMetalLayer*)surface.layer;
    layer.device = device;
    layer.pixelFormat = kColorFormat;
    layer.framebufferOnly = YES;
    layer.maximumDrawableCount = kDrawables;
    layer.opaque = surface.transparent ? NO : YES;
#if TARGET_OS_OSX || TARGET_OS_MACCATALYST
    layer.displaySyncEnabled = surface.vsync ? YES : NO;
#endif
    resize(surface.width, surface.height);
}

// The frame that records may draw the drawable already, which then waits for the frame in the graveyard.
void Swapchain::detach() {
    graveyard.retire(drawable);
    layer = nil;
    drawable = nil;
}

void Swapchain::resize(int pixelWidth, int pixelHeight) noexcept {
    width = pixelWidth;
    height = pixelHeight;
}

bool Swapchain::acquire() {
    if (drawable != nil) {
        return true;
    }
    if (layer == nil || width <= 0 || height <= 0) {
        return false;
    }

    const CGSize size = CGSizeMake(width, height);
    if (!CGSizeEqualToSize(layer.drawableSize, size)) {
        layer.drawableSize = size;
    }
    @autoreleasepool {
        drawable = [layer nextDrawable];
    }
    if (drawable == nil) {
        return false;
    }

    makeAttachments(drawable.texture.width, drawable.texture.height);
    return true;
}

void Swapchain::present(id<MTLCommandBuffer> commands) {
    if (drawable != nil) {
        [commands presentDrawable:drawable];
        drawable = nil;
    }
}

// The attachments of the old size may still be drawn by frames in flight, so they wait for them in the graveyard.
void Swapchain::makeAttachments(NSUInteger pixelWidth, NSUInteger pixelHeight) {
    if (depth != nil && depth.width == pixelWidth && depth.height == pixelHeight) {
        return;
    }

    graveyard.retire(depth);
    graveyard.retire(multisample);
    depth = makeAttachment(kDepthFormat, sampleCount, pixelWidth, pixelHeight, @"haylen-swapchain-depth");
    multisample = sampleCount > 1 ? makeAttachment(kColorFormat, sampleCount, pixelWidth, pixelHeight, @"haylen-swapchain-color") : nil;
}

id<MTLTexture> Swapchain::makeAttachment(MTLPixelFormat format, int samples, NSUInteger pixelWidth, NSUInteger pixelHeight, NSString* label) {
    MTLTextureDescriptor* desc = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:format width:pixelWidth height:pixelHeight mipmapped:NO];
    desc.textureType = samples > 1 ? MTLTextureType2DMultisample : MTLTextureType2D;
    desc.sampleCount = static_cast<NSUInteger>(samples);
    desc.usage = MTLTextureUsageRenderTarget;
    desc.storageMode = memoryless ? MTLStorageModeMemoryless : MTLStorageModePrivate;

    id<MTLTexture> texture = [device newTextureWithDescriptor:desc];
    if (texture == nil) {
        throw std::runtime_error(std::format("The GPU has no memory for the attachments of a swapchain of {}x{} pixels.", pixelWidth, pixelHeight));
    }
    texture.label = label;
    return texture;
}

} // namespace haylen::gpu::metal
