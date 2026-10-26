#pragma once

#include <webgpu/webgpu.h>

#include <string>

#include "haylen/gpu/Device.hpp"
#include "haylen/gpu/Surface.hpp"

namespace haylen::gpu::webgpu {

// The canvas of the page that the frames are drawn into, configured for the device in the format the browser prefers, with the depth and stencil attachment of the frames and the multisampled color attachment that resolves into the canvas when the window asks for more than one sample. The browser presents the texture of the canvas when the frame returns to its event loop, at the pace of the display, so frames never run without vertical sync.
class Swapchain final {
  public:
    static constexpr Device::PixelFormat kDepthFormat = Device::PixelFormat::Depth24Stencil8;

    Swapchain(WGPUInstance instance, WGPUAdapter adapter, WGPUDevice owner, const Surface& target);
    Swapchain(const Swapchain&) = delete;
    Swapchain& operator=(const Swapchain&) = delete;
    ~Swapchain();

    // Takes the new size of the canvas in pixels, which the next frame configures.
    void resize(int pixelsWide, int pixelsHigh) noexcept;

    // Takes the texture of the canvas for the frame, once per frame, and returns whether the frame has one, which a canvas without pixels lacks.
    [[nodiscard]] bool acquire();

    // Lets go of the texture of the frame once the frame is submitted.
    void release() noexcept;

    [[nodiscard]] WGPUTextureView getColorView() const noexcept;
    [[nodiscard]] WGPUTextureView getResolveView() const noexcept;
    [[nodiscard]] WGPUTextureView getDepthView() const noexcept;
    [[nodiscard]] Device::PixelFormat getFormat() const noexcept;
    [[nodiscard]] int getSampleCount() const noexcept;
    [[nodiscard]] int getWidth() const noexcept;
    [[nodiscard]] int getHeight() const noexcept;

  private:
    [[nodiscard]] static Device::PixelFormat toPixelFormat(WGPUTextureFormat value) noexcept;
    [[nodiscard]] WGPUTexture makeAttachment(WGPUTextureFormat attachmentFormat, const char* label) const;

    void configure();
    void releaseAttachments() noexcept;

    WGPUDevice device;
    WGPUSurface surface = nullptr;
    std::string canvas;
    WGPUTextureFormat format = WGPUTextureFormat_BGRA8Unorm;
    WGPUCompositeAlphaMode alphaMode = WGPUCompositeAlphaMode_Opaque;
    int sampleCount = 1;
    int width = 0;
    int height = 0;
    bool configured = false;
    bool stale = true;
    WGPUTexture frameTexture = nullptr;
    WGPUTextureView frameView = nullptr;
    WGPUTexture multisampled = nullptr;
    WGPUTextureView multisampledView = nullptr;
    WGPUTexture depth = nullptr;
    WGPUTextureView depthView = nullptr;
};

} // namespace haylen::gpu::webgpu
