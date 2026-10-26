#include "gpu/webgpu/Swapchain.hpp"

#include <cstdint>
#include <stdexcept>
#include <utility>

#include "gpu/webgpu/Conversion.hpp"

namespace haylen::gpu::webgpu {

// The first format the surface reports is the one the browser prefers for the canvas, which draws without a conversion when the page composites it.
Swapchain::Swapchain(WGPUInstance instance, WGPUAdapter adapter, WGPUDevice owner, const Surface& target) : device(owner), canvas(target.canvas), sampleCount(target.sampleCount), width(target.width), height(target.height) {
    if (target.kind != Surface::Kind::WebCanvas) {
        throw std::runtime_error("The WebGPU device draws only into the canvas of a page.");
    }
    if (sampleCount != 1 && sampleCount != 4) {
        throw std::runtime_error("WebGPU draws into a canvas with one or four samples.");
    }

    WGPUEmscriptenSurfaceSourceCanvasHTMLSelector source = WGPU_EMSCRIPTEN_SURFACE_SOURCE_CANVAS_HTML_SELECTOR_INIT;
    source.selector = Conversion::toText(canvas);
    WGPUSurfaceDescriptor descriptor = WGPU_SURFACE_DESCRIPTOR_INIT;
    descriptor.nextInChain = &source.chain;
    descriptor.label = Conversion::toText("haylen-canvas");
    surface = wgpuInstanceCreateSurface(instance, &descriptor);
    if (surface == nullptr) {
        throw std::runtime_error("The canvas of the page gave no WebGPU context.");
    }

    WGPUSurfaceCapabilities capabilities = WGPU_SURFACE_CAPABILITIES_INIT;
    if (wgpuSurfaceGetCapabilities(surface, adapter, &capabilities) == WGPUStatus_Success && capabilities.formatCount > 0) {
        format = capabilities.formats[0];
        wgpuSurfaceCapabilitiesFreeMembers(capabilities);
    }
    alphaMode = target.transparent ? WGPUCompositeAlphaMode_Premultiplied : WGPUCompositeAlphaMode_Opaque;
}

Swapchain::~Swapchain() {
    release();
    releaseAttachments();
    if (configured) {
        wgpuSurfaceUnconfigure(surface);
    }
    wgpuSurfaceRelease(surface);
}

Device::PixelFormat Swapchain::toPixelFormat(WGPUTextureFormat value) noexcept {
    switch (value) {
    case WGPUTextureFormat_RGBA8Unorm:
        return Device::PixelFormat::Rgba8;
    case WGPUTextureFormat_RGBA16Float:
        return Device::PixelFormat::Rgba16Float;
    default:
        return Device::PixelFormat::Bgra8;
    }
}

void Swapchain::resize(int pixelsWide, int pixelsHigh) noexcept {
    if (pixelsWide == width && pixelsHigh == height) {
        return;
    }
    width = pixelsWide;
    height = pixelsHigh;
    stale = true;
}

WGPUTexture Swapchain::makeAttachment(WGPUTextureFormat attachmentFormat, const char* label) const {
    WGPUTextureDescriptor descriptor = WGPU_TEXTURE_DESCRIPTOR_INIT;
    descriptor.label = Conversion::toText(label);
    descriptor.usage = WGPUTextureUsage_RenderAttachment;
    descriptor.size = {.width = static_cast<std::uint32_t>(width), .height = static_cast<std::uint32_t>(height), .depthOrArrayLayers = 1};
    descriptor.format = attachmentFormat;
    descriptor.sampleCount = static_cast<std::uint32_t>(sampleCount);
    return wgpuDeviceCreateTexture(device, &descriptor);
}

// Configuring the surface sets the size of the drawing buffer of the canvas, and the attachments of the frames follow it.
void Swapchain::configure() {
    releaseAttachments();
    stale = false;
    if (width <= 0 || height <= 0) {
        return;
    }

    WGPUSurfaceConfiguration configuration = WGPU_SURFACE_CONFIGURATION_INIT;
    configuration.device = device;
    configuration.format = format;
    configuration.usage = WGPUTextureUsage_RenderAttachment;
    configuration.width = static_cast<std::uint32_t>(width);
    configuration.height = static_cast<std::uint32_t>(height);
    configuration.alphaMode = alphaMode;
    configuration.presentMode = WGPUPresentMode_Fifo;
    wgpuSurfaceConfigure(surface, &configuration);
    configured = true;

    depth = makeAttachment(Conversion::toTextureFormat(kDepthFormat), "haylen-swapchain-depth");
    depthView = wgpuTextureCreateView(depth, nullptr);
    if (sampleCount > 1) {
        multisampled = makeAttachment(format, "haylen-swapchain-color");
        multisampledView = wgpuTextureCreateView(multisampled, nullptr);
    }
}

bool Swapchain::acquire() {
    if (frameView != nullptr) {
        return true;
    }
    if (stale) {
        configure();
    }
    if (depth == nullptr) {
        return false;
    }

    WGPUSurfaceTexture current = WGPU_SURFACE_TEXTURE_INIT;
    wgpuSurfaceGetCurrentTexture(surface, &current);
    if (current.status != WGPUSurfaceGetCurrentTextureStatus_SuccessOptimal && current.status != WGPUSurfaceGetCurrentTextureStatus_SuccessSuboptimal) {
        if (current.texture != nullptr) {
            wgpuTextureRelease(current.texture);
        }
        stale = true;
        return false;
    }
    frameTexture = current.texture;
    frameView = wgpuTextureCreateView(frameTexture, nullptr);
    return true;
}

void Swapchain::release() noexcept {
    if (frameView == nullptr) {
        return;
    }
    wgpuTextureViewRelease(frameView);
    wgpuTextureRelease(frameTexture);
    frameView = nullptr;
    frameTexture = nullptr;
}

// The attachments of the last frame may still be in use by the GPU, which WebGPU waits for before it frees a destroyed texture.
void Swapchain::releaseAttachments() noexcept {
    for (auto [texture, view] : {std::pair{&depth, &depthView}, std::pair{&multisampled, &multisampledView}}) {
        if (*texture == nullptr) {
            continue;
        }
        wgpuTextureViewRelease(*view);
        wgpuTextureDestroy(*texture);
        wgpuTextureRelease(*texture);
        *texture = nullptr;
        *view = nullptr;
    }
}

WGPUTextureView Swapchain::getColorView() const noexcept {
    return multisampledView != nullptr ? multisampledView : frameView;
}

WGPUTextureView Swapchain::getResolveView() const noexcept {
    return multisampledView != nullptr ? frameView : nullptr;
}

WGPUTextureView Swapchain::getDepthView() const noexcept {
    return depthView;
}

Device::PixelFormat Swapchain::getFormat() const noexcept {
    return toPixelFormat(format);
}

int Swapchain::getSampleCount() const noexcept {
    return sampleCount;
}

int Swapchain::getWidth() const noexcept {
    return width;
}

int Swapchain::getHeight() const noexcept {
    return height;
}

} // namespace haylen::gpu::webgpu
