#include "platform/android/AndroidGraphics.hpp"

#include <exception>
#include <stdexcept>

#include "gpu/DeviceFactory.hpp"
#include "haylen/core/Log.hpp"

namespace haylen::platform {

std::unique_ptr<gpu::Device> AndroidGraphics::createDevice() {
    for (const gpu::Device::Api api : apis.getOrder()) {
        if (apis.hasFailed(api)) {
            continue;
        }
        try {
            std::unique_ptr<gpu::Device> device = open(api);
            core::Log::info("The app draws through \"{}\".", gpu::DeviceFactory::getApiName(api));
            return device;
        } catch (const std::exception& error) {
            apis.fail(api);
            core::Log::warning("The graphics API \"{}\" could not start on this device. {}", gpu::DeviceFactory::getApiName(api), error.what());
        }
    }
    throw std::runtime_error("No graphics API of this build starts on this device.");
}

void AndroidGraphics::attachWindow(gpu::Device* device) {
    ANativeWindow* native = window.getNative();
    if (egl != nullptr) {
        egl->attachWindow(*native);
    }
    if (device != nullptr) {
        device->attachSurface(*describeSurface(device->getApi()));
    }
}

bool AndroidGraphics::isLost(const gpu::Device& device) const noexcept {
    const bool contextLost = gpu::DeviceFactory::usesGlContext(device.getApi()) && egl != nullptr && egl->isLost();
    return device.isLost() || contextLost;
}

void AndroidGraphics::resizeWindow(gpu::Device* device) {
    if (device != nullptr) {
        device->resizeSurface(window.getFramebufferWidth(), window.getFramebufferHeight());
    }
}

// The device lets go of its swapchain before the window surface of EGL ends, both before the activity loses the window.
void AndroidGraphics::detachWindow(gpu::Device* device) noexcept {
    if (device != nullptr) {
        device->detachSurface();
    }
    if (egl != nullptr) {
        egl->releaseWindow();
    }
}

// A window takes the frames of one graphics API at a time, so a device that draws into it directly first ends the context of OpenGL ES that an earlier app drew through. A device of OpenGL ES draws through a new context once the GPU lost the one before, which its device or EGL reported, and one made while the activity has no window waits for one, like a device whose window went.
std::unique_ptr<gpu::Device> AndroidGraphics::open(gpu::Device::Api api) {
    const gpu::DeviceFactory::Options options{.validation = kValidation};
    if (!gpu::DeviceFactory::usesGlContext(api)) {
        egl.reset();
        return gpu::DeviceFactory::create(api, describeSurface(api), options);
    }

    const bool renewed = apis.takeRenewal(api);
    if (renewed || egl == nullptr || egl->isLost()) {
        egl.reset();
        egl = std::make_unique<AndroidEglContext>(settings.sampleCount, kValidation);
    }
    ANativeWindow* native = window.getNative();
    if (native != nullptr) {
        egl->attachWindow(*native);
    }
    std::unique_ptr<gpu::Device> device = gpu::DeviceFactory::create(api, describeSurface(api), options);
    if (native == nullptr) {
        device->detachSurface();
    }
    return device;
}

// A device that draws into the window directly has no surface while the activity has no window, and one of OpenGL ES always draws through the context.
std::optional<gpu::Surface> AndroidGraphics::describeSurface(gpu::Device::Api api) const {
    const int width = window.getFramebufferWidth();
    const int height = window.getFramebufferHeight();
    if (gpu::DeviceFactory::usesGlContext(api)) {
        return gpu::Surface{.kind = gpu::Surface::Kind::Gl, .gl = egl.get(), .width = width, .height = height, .sampleCount = settings.sampleCount, .vsync = settings.vsync};
    }
    ANativeWindow* native = window.getNative();
    if (native == nullptr) {
        return std::nullopt;
    }
    return gpu::Surface{.kind = gpu::Surface::Kind::Android, .window = native, .width = width, .height = height, .sampleCount = settings.sampleCount, .vsync = settings.vsync};
}

} // namespace haylen::platform
