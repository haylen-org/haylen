#include "gpu/metal/Swapchain.hpp"

#import <QuartzCore/CAMetalDisplayLink.h>

#include <format>
#include <stdexcept>
#include <utility>

#import "gpu/metal/HaylenDisplayLinkDelegate.h"

namespace haylen::gpu::metal {

Swapchain::Swapchain(id<MTLDevice> metal, Graveyard& retired, int samples, bool tileMemory) : device(metal), graveyard(retired), sampleCount(samples), memoryless(tileMemory) {}

Swapchain::~Swapchain() {
    stopLink();
    graveyard.retire(drawable);
    graveyard.retire(depth);
    graveyard.retire(multisample);
}

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
    resize(surface.size);

    if (paced) {
        startLink();
    }
}

// The frame that records may draw the drawable already, which then waits for the frame in the graveyard.
void Swapchain::detach() {
    stopLink();
    graveyard.retire(drawable);
    layer = nil;
    drawable = nil;
    drawn = false;
}

// The layer stretches its drawable over its bounds, so a drawable smaller than the window shows at the size of the window, and the window size needs nothing of the device.
void Swapchain::resize(const Surface::Size& size) noexcept {
    width = size.width;
    height = size.height;

    // The display link offers drawables of the size of the layer, which takes the new size for the next one.
    if (link != nil && width > 0 && height > 0) {
        layer.drawableSize = CGSizeMake(width, height);
    }
}

bool Swapchain::pace(int framesPerSecond, std::function<void()> due) {
    if (@available(macOS 14.0, iOS 17.0, tvOS 17.0, *)) {
        paced = std::move(due);
        pacedRate = framesPerSecond;
        if (!paced) {
            stopLink();
            return true;
        }

        if (layer != nil) {
            startLink();
        }

        return true;
    }

    return false;
}

// The display link offers a drawable once the display took the frame before, at the latency of one frame, the least it allows, so the frame that the drawable comes with shows at the next refresh it makes. It runs in every mode of the run loop, so the frames that SDL plays during a live resize of a macOS window come at the refreshes too.
void Swapchain::startLink() {
    if (@available(macOS 14.0, iOS 17.0, tvOS 17.0, *)) {
        if (link == nil) {
            // clang-format off
            HaylenDisplayLinkDelegate* delegate = [[HaylenDisplayLinkDelegate alloc] initWithUpdate:[this](id<CAMetalDrawable> offered) {
                receive(offered);
            }];
            // clang-format on
            CAMetalDisplayLink* made = [[CAMetalDisplayLink alloc] initWithMetalLayer:layer];
            made.delegate = delegate;
            made.preferredFrameLatency = 1.0F;
            [made addToRunLoop:NSRunLoop.mainRunLoop forMode:NSRunLoopCommonModes];
            link = made;
            linkDelegate = delegate;
        }

        if (width > 0 && height > 0) {
            layer.drawableSize = CGSizeMake(width, height);
        }

        const auto rate = static_cast<float>(pacedRate);
        CAMetalDisplayLink* current = link;
        current.preferredFrameRateRange = pacedRate > 0 ? CAFrameRateRangeMake(rate, rate, rate) : CAFrameRateRangeDefault;
    }
}

// A drawable that the display link offered and no frame drew goes back to the layer at once.
void Swapchain::stopLink() noexcept {
    if (link == nil) {
        return;
    }

    [link invalidate];
    link = nil;
    linkDelegate = nil;

    if (!drawn) {
        drawable = nil;
    }
}

// The drawable of an update that no frame drew goes back to the layer as the next one comes, so frames that draw nothing, such as those of an app under a native screen, never hold the drawables of the layer. A drawable that a frame drew and has not presented yet, as while the debugger holds the frame, stays with that frame, and the frame that the call plays then draws nothing into the window.
void Swapchain::receive(id<CAMetalDrawable> offered) {
    if (!drawn) {
        drawable = offered;
    }

    paced();
}

// A drawable that no frame drew holds no command of the GPU, so one that the wait for the display or the display link took before the surface resized goes back to the layer at once, which keeps the layer from running out of drawables. A frame keeps the drawable it drew until it presents it. While the display link paces the window, the drawables come from it alone.
void Swapchain::prepare() {
    const bool fits = drawable != nil && static_cast<int>(drawable.texture.width) == width && static_cast<int>(drawable.texture.height) == height;
    if (fits || drawn) {
        return;
    }

    drawable = nil;

    if (link != nil || layer == nil || width <= 0 || height <= 0) {
        return;
    }

    const CGSize size = CGSizeMake(width, height);
    if (!CGSizeEqualToSize(layer.drawableSize, size)) {
        layer.drawableSize = size;
    }

    @autoreleasepool {
        drawable = [layer nextDrawable];
    }
}

// The attachments follow the drawable of the frame, also one that the display link offered.
bool Swapchain::acquire() {
    prepare();
    drawn = drawable != nil;
    if (drawn) {
        makeAttachments(drawable.texture.width, drawable.texture.height);
    }

    return drawn;
}

// A drawable that the frame did not draw stays for the next frame.
bool Swapchain::present(id<MTLCommandBuffer> commands) {
    if (!std::exchange(drawn, false)) {
        return false;
    }

    [commands presentDrawable:drawable];
    drawable = nil;
    return true;
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
