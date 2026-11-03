#pragma once

#include <atomic>
#include <memory>

#include <SDL3/SDL_metal.h>
#include <SDL3/SDL_video.h>

#include "haylen/core/AppConfig.hpp"
#include "haylen/gpu/Device.hpp"
#include "haylen/gpu/Surface.hpp"
#include "haylen/platform/Display.hpp"
#include "platform/NativeWindow.hpp"
#include "platform/sdl/GlContext.hpp"

namespace haylen::platform::sdl {

// The window that SDL opens for the app, made for the graphics API that draws into it, as the system of each platform sees it. One object stands for the window of the process, which opens again whenever the runtime tries another graphics API whose window needs other flags.
class Window final : public platform::NativeWindow {
  public:
    Window() = default;
    ~Window() override;

    // Opens the window hidden, with the options of the app and what the graphics API draws through: a Metal layer, a Vulkan surface or a context of OpenGL. Throws `std::runtime_error` when the system cannot make such a window, so the runtime tries the next graphics API.
    void open(const core::AppConfig::Window& options, gpu::Device::Api api);
    void close() noexcept;
    void show();

    [[nodiscard]] SDL_Window* get() const noexcept {
        return window;
    }

    [[nodiscard]] gpu::Device::Api getApi() const noexcept {
        return api;
    }

    [[nodiscard]] gpu::Surface getSurface() const;

    // Replaces the context of OpenGL, OpenGL ES or WebGL2 that the GPU lost with a new one for the device that replaces the lost one. Windows of the other graphics APIs keep what they have.
    void renewGlContext();

    // Reads the size of the drawable and the scale again once SDL reported that they changed, for the services that read them on other threads.
    void refresh() noexcept;

    // The size of the drawable in pixels, and the framebuffer pixels per unit of the coordinates SDL gives the window, which turns the positions of the mouse into framebuffer pixels.
    [[nodiscard]] math::Vec2 getPixelSize() const noexcept;
    [[nodiscard]] float getPixelDensity() const noexcept;

    [[nodiscard]] int getSampleCount() const noexcept {
        return sampleCount;
    }

    [[nodiscard]] bool isVsync() const noexcept {
        return vsync;
    }

    // The surface of the window of the system that SDL opened, with the layer of its Metal view where it has one, for the graphics APIs that draw into the window itself.
    [[nodiscard]] static gpu::Surface describe(SDL_Window* described, SDL_MetalView view);

    // Returns the display of SDL at the place of a display of the desktop. Throws `std::runtime_error` when SDL knows no display there.
    [[nodiscard]] static SDL_DisplayID findDisplay(const Display& display);

  protected:
    [[nodiscard]] bool queryOpen() const noexcept override;
    [[nodiscard]] void* queryHandle() const noexcept override;
    [[nodiscard]] void* queryDisplay() const noexcept override;
    [[nodiscard]] void* queryActivity() const noexcept override;
    [[nodiscard]] math::Vec2 queryFramebufferSize() const noexcept override;
    [[nodiscard]] float queryScale() const noexcept override;

  private:
    [[nodiscard]] static SDL_WindowFlags getFlags(const core::AppConfig::Window& options, gpu::Device::Api api) noexcept;
    static void describeGl(const core::AppConfig::Window& options, gpu::Device::Api api);
    void attachApi();
    void readHandles() noexcept;
    [[nodiscard]] gpu::Surface getNativeSurface() const;

    SDL_Window* window = nullptr;
    gpu::Device::Api api = gpu::Device::Api::None;
    SDL_MetalView metalView = nullptr;
    std::unique_ptr<GlContext> gl;
    int sampleCount = 1;
    bool vsync = true;
    bool transparent = false;

    // What other threads read: the native handles of the window, the size of its drawable in pixels and its scale.
    std::atomic<bool> opened = false;
    std::atomic<void*> handle = nullptr;
    std::atomic<void*> display = nullptr;
    std::atomic<float> width = 0.0F;
    std::atomic<float> height = 0.0F;
    std::atomic<float> scale = 1.0F;
};

} // namespace haylen::platform::sdl
