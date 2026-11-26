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

    // The drawables a layer keeps, the one that the display shows and the one that the next frame draws, so no frame waits to be shown behind another and its input reaches the screen at the next refresh it makes.
    static constexpr NSUInteger kDrawables = 2;

    Swapchain(id<MTLDevice> metal, Graveyard& retired, int samples, bool tileMemory);

    // The frames in flight may still draw the drawable and the attachments, which then wait for them in the graveyard.
    ~Swapchain();
    Swapchain(const Swapchain&) = delete;
    Swapchain& operator=(const Swapchain&) = delete;

    // Draws into the layer of the surface from the next frame. Throws `std::runtime_error` when the surface holds no `CAMetalLayer` or asks for a sample count other than the one the swapchain pipelines of the device were made for.
    void attach(const Surface& surface);
    void detach();
    void resize(const Surface::Size& size) noexcept;

    [[nodiscard]] int getSampleCount() const noexcept {
        return sampleCount;
    }

    // Whether the multisampled color and the depth and stencil attachments live in tile memory alone, so no pass can load them.
    [[nodiscard]] bool isMemoryless() const noexcept {
        return memoryless;
    }

    // Takes the drawable of the next frame ahead of its passes, which it keeps until a frame draws and presents it, and the size of the surface replaces one that it took at another size. A surface without a layer or without pixels takes none, and neither does a layer that gave no drawable in time.
    void prepare();

    // Takes the drawable that the swapchain pass of the frame draws into, which the commit of the frame presents. It fails where `prepare` takes none.
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

    // Presents the drawable of the frame once the command buffer finished drawing it, and gives whether the frame drew one.
    bool present(id<MTLCommandBuffer> commands);

  private:
    void makeAttachments(NSUInteger pixelWidth, NSUInteger pixelHeight);
    [[nodiscard]] id<MTLTexture> makeAttachment(MTLPixelFormat format, int samples, NSUInteger pixelWidth, NSUInteger pixelHeight, NSString* label);

    id<MTLDevice> device = nil;
    Graveyard& graveyard;
    CAMetalLayer* layer = nil;
    id<CAMetalDrawable> drawable = nil;

    // Whether a pass of the frame that records drew the drawable.
    bool drawn = false;
    id<MTLTexture> multisample = nil;
    id<MTLTexture> depth = nil;
    int width = 0;
    int height = 0;
    int sampleCount = 1;
    bool memoryless = false;
};

} // namespace haylen::gpu::metal
