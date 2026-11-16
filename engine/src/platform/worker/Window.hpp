#pragma once

#include <memory>

#include "haylen/core/AppConfig.hpp"
#include "haylen/gpu/Device.hpp"
#include "haylen/gpu/Surface.hpp"
#include "haylen/math/Vec2.hpp"
#include "platform/NativeWindow.hpp"
#include "platform/worker/GlContext.hpp"

namespace haylen::platform::worker {

// The window of the app in the Worker mode: the `OffscreenCanvas` that the page handed the worker, whose drawing buffer the worker sizes, while the page lays out the canvas element and reports its size in CSS pixels and the pixels of the screen in each of them.
class Window final : public platform::NativeWindow {
  public:
    Window() = default;
    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;
    ~Window() override;

    // Prepares the canvas for the graphics API: a WebGL2 context for WebGL2, while WebGPU makes its own context on the canvas. Throws `std::runtime_error` when the canvas gives no such context, so the runtime tries the next graphics API.
    void open(const core::AppConfig::Window& options, gpu::Device::Api value);
    void close() noexcept;

    [[nodiscard]] gpu::Device::Api getApi() const noexcept {
        return api;
    }

    [[nodiscard]] gpu::Surface getSurface() const;

    // Replaces the WebGL2 context that the GPU lost with a new one for the device that replaces the lost one.
    void renewGlContext();

    // Reads the size of the canvas box that the page reported and sizes the drawable after it and the pixel density cap, with the drawing buffer of a WebGL2 canvas, which WebGPU sizes as it configures the canvas.
    void refresh();

    void setMaxPixelDensity(float value) noexcept {
        maxPixelDensity = value;
    }

    [[nodiscard]] math::Vec2 getPixelSize() const noexcept;

    // The framebuffer pixels per CSS pixel of the canvas box, which turns the positions that the page reports into framebuffer pixels.
    [[nodiscard]] float getPixelDensity() const noexcept {
        return scale;
    }

    [[nodiscard]] gpu::Surface::Size getSurfaceSize() const noexcept {
        return size;
    }

    [[nodiscard]] int getSampleCount() const noexcept {
        return sampleCount;
    }

    [[nodiscard]] bool isTransparent() const noexcept {
        return transparent;
    }

  protected:
    [[nodiscard]] bool queryOpen() const noexcept override;
    [[nodiscard]] void* queryHandle() const noexcept override;
    [[nodiscard]] void* queryDisplay() const noexcept override;
    [[nodiscard]] void* queryActivity() const noexcept override;
    [[nodiscard]] math::Vec2 queryFramebufferSize() const noexcept override;
    [[nodiscard]] float queryScale() const noexcept override;

  private:
    core::AppConfig::Window options;
    gpu::Device::Api api = gpu::Device::Api::None;
    std::unique_ptr<GlContext> gl;
    int sampleCount = 1;
    bool vsync = true;
    bool transparent = false;
    bool opened = false;
    float maxPixelDensity = 0.0F;
    gpu::Surface::Size size;
    float scale = 1.0F;
};

} // namespace haylen::platform::worker
