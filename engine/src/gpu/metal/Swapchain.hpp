#pragma once

#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>

#include "gpu/metal/Graveyard.hpp"
#include "haylen/gpu/Surface.hpp"

namespace haylen::gpu::metal {

// The drawables of the `CAMetalLayer` that the platform made for the window, with the multisampled color and the depth and stencil attachments that swapchain passes draw into at the size of the drawable. Tile GPUs keep those attachments in tile memory alone, since no pass stores them.
class Swapchain final {
  public:
    static constexpr MTLPixelFormat kColorFormat = MTLPixelFormatBGRA8Unorm;
    static constexpr MTLPixelFormat kDepthFormat = MTLPixelFormatDepth32Float_Stencil8;

    // The drawables a layer keeps, one for each frame in flight.
    static constexpr NSUInteger kDrawables = 3;

    Swapchain(id<MTLDevice> metal, Graveyard& retired, int samples, bool tileMemory);

    // Draws into the layer of the surface from the next frame. Throws `std::runtime_error` when the surface holds no `CAMetalLayer` or asks for a sample count other than the one the swapchain pipelines of the device were made for.
    void attach(const Surface& surface);
    void detach();
    void resize(int pixelWidth, int pixelHeight) noexcept;

    [[nodiscard]] int getSampleCount() const noexcept {
        return sampleCount;
    }

    // Whether the multisampled color and the depth and stencil attachments live in tile memory alone, so no pass can load them.
    [[nodiscard]] bool isMemoryless() const noexcept {
        return memoryless;
    }

    // Takes the drawable of the frame, once a frame. It fails without a surface, while the surface has no pixels and when the layer gave no drawable in time.
    [[nodiscard]] bool acquire();

    [[nodiscard]] id<CAMetalDrawable> getDrawable() const noexcept {
        return drawable;
    }

    [[nodiscard]] id<MTLTexture> getMultisampleTexture() const noexcept {
        return multisample;
    }

    [[nodiscard]] id<MTLTexture> getDepthTexture() const noexcept {
        return depth;
    }

    // Presents the drawable of the frame once the command buffer finished drawing it.
    void present(id<MTLCommandBuffer> commands);

  private:
    void makeAttachments(NSUInteger pixelWidth, NSUInteger pixelHeight);
    [[nodiscard]] id<MTLTexture> makeAttachment(MTLPixelFormat format, int samples, NSUInteger pixelWidth, NSUInteger pixelHeight, NSString* label);

    id<MTLDevice> device = nil;
    Graveyard& graveyard;
    CAMetalLayer* layer = nil;
    id<CAMetalDrawable> drawable = nil;
    id<MTLTexture> multisample = nil;
    id<MTLTexture> depth = nil;
    int width = 0;
    int height = 0;
    int sampleCount = 1;
    bool memoryless = false;
};

} // namespace haylen::gpu::metal
