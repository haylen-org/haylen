#include "gpu/webgpu/Swapchain.hpp"

#include <cstdint>
#include <stdexcept>
#include <utility>

#include <emscripten/emscripten.h>

#include "gpu/webgpu/Conversion.hpp"

// The browser prefers one format for every canvas of the page, `rgba8unorm` or `bgra8unorm`, which it names as text.
// clang-format off
EM_JS(int, haylen_js_prefers_rgba_canvas, (), {
    return navigator.gpu.getPreferredCanvasFormat() === "rgba8unorm" ? 1 : 0;
});
// clang-format on

namespace haylen::gpu::webgpu {

Swapchain::Swapchain(WGPUInstance instance, WGPUDevice owner, const Surface& target, Device::PixelFormat canvasFormat, int samples) : device(owner), canvas(target.canvas), format(canvasFormat), sampleCount(samples), size(target.size) {
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

    alphaMode = target.transparent ? WGPUCompositeAlphaMode_Premultiplied : WGPUCompositeAlphaMode_Opaque;
}

Swapchain::~Swapchain() {
    release();
    releaseAttachments();
    scaled.reset();

    if (configured) {
        wgpuSurfaceUnconfigure(surface);
    }

    wgpuSurfaceRelease(surface);
}

Device::PixelFormat Swapchain::getPreferredFormat() {
    return haylen_js_prefers_rgba_canvas() == 1 ? Device::PixelFormat::Rgba8 : Device::PixelFormat::Bgra8;
}

void Swapchain::resize(const Surface::Size& value) noexcept {
    if (value == size) {
        return;
    }

    size = value;
    stale = true;
}

void Swapchain::setSampleCount(int value) noexcept {
    if (value == sampleCount) {
        return;
    }

    sampleCount = value;
    stale = true;
}

WGPUTexture Swapchain::makeAttachment(WGPUTextureFormat attachmentFormat, const char* label) const {
    WGPUTextureDescriptor descriptor = WGPU_TEXTURE_DESCRIPTOR_INIT;
    descriptor.label = Conversion::toText(label);
    descriptor.usage = WGPUTextureUsage_RenderAttachment;
    descriptor.size = {.width = static_cast<std::uint32_t>(size.width), .height = static_cast<std::uint32_t>(size.height), .depthOrArrayLayers = 1};
    descriptor.format = attachmentFormat;
    descriptor.sampleCount = static_cast<std::uint32_t>(sampleCount);
    return wgpuDeviceCreateTexture(device, &descriptor);
}

// Configuring the surface sets the size of the drawing buffer of the canvas to the size of the window, and the attachments of the frames follow the drawable.
void Swapchain::configure() {
    releaseAttachments();
    stale = false;
    configuredSize = size;

    if (size.width <= 0 || size.height <= 0) {
        return;
    }

    WGPUSurfaceConfiguration configuration = WGPU_SURFACE_CONFIGURATION_INIT;
    configuration.device = device;
    configuration.format = Conversion::toTextureFormat(format);
    configuration.usage = WGPUTextureUsage_RenderAttachment;
    configuration.width = static_cast<std::uint32_t>(size.windowWidth);
    configuration.height = static_cast<std::uint32_t>(size.windowHeight);
    configuration.alphaMode = alphaMode;
    configuration.presentMode = WGPUPresentMode_Fifo;
    wgpuSurfaceConfigure(surface, &configuration);
    configured = true;

    depth = makeAttachment(Conversion::toTextureFormat(kDepthFormat), "haylen-swapchain-depth");
    depthView = wgpuTextureCreateView(depth, nullptr);

    if (sampleCount > 1) {
        multisampled = makeAttachment(Conversion::toTextureFormat(format), "haylen-swapchain-color");
        multisampledView = wgpuTextureCreateView(multisampled, nullptr);
    }

    if (!size.isScaled()) {
        scaled.reset();
        return;
    }

    if (scaled == nullptr) {
        scaled = std::make_unique<ScaledDrawable>(device, Conversion::toTextureFormat(format), size);
        return;
    }

    scaled->resize(size);
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

void Swapchain::recordScale(WGPUCommandEncoder encoder) const {
    if (scaled != nullptr && frameView != nullptr) {
        scaled->record(encoder, frameView);
    }
}

WGPUTextureView Swapchain::getColorView() const noexcept {
    if (multisampledView != nullptr) {
        return multisampledView;
    }

    return scaled != nullptr ? scaled->getView() : frameView;
}

WGPUTextureView Swapchain::getResolveView() const noexcept {
    if (multisampledView == nullptr) {
        return nullptr;
    }

    return scaled != nullptr ? scaled->getView() : frameView;
}

WGPUTextureView Swapchain::getDepthView() const noexcept {
    return depthView;
}

Device::PixelFormat Swapchain::getFormat() const noexcept {
    return format;
}

int Swapchain::getSampleCount() const noexcept {
    return sampleCount;
}

int Swapchain::getWidth() const noexcept {
    return configuredSize.width;
}

int Swapchain::getHeight() const noexcept {
    return configuredSize.height;
}

} // namespace haylen::gpu::webgpu
