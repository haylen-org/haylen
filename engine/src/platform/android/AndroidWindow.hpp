#pragma once

#include <android/native_window.h>
#include <game-activity/GameActivity.h>

#include <atomic>

#include "haylen/gpu/Surface.hpp"
#include "platform/NativeWindow.hpp"

namespace haylen::platform {

// The window of an Android activity: its `GameActivity` and the `ANativeWindow` of its surface while one exists, with the size of the framebuffer. An app that caps its pixel density on a denser screen draws into buffers of fewer pixels than the window, which the system stretches over it. The frame thread changes it, and services on other threads read it.
class AndroidWindow final : public NativeWindow {
  public:
    explicit AndroidWindow(GameActivity& activity) noexcept : native(activity) {}

    // Caps the framebuffer pixels in each point of the window, or takes every pixel of the window for 0, from the next `open` or `refresh`.
    void setMaxPixelDensity(float value) noexcept {
        maxPixelDensity = value;
    }

    void setRunning(bool value) noexcept {
        running.store(value);
    }

    // Takes the window of the surface, new or of a new size in pixels of the window, and sizes its buffers as the framebuffer.
    void open(ANativeWindow& window, int width, int height) noexcept;

    // Sizes the buffers of the window that is open again, after the pixel density cap changed.
    void refresh() noexcept;

    // Lets go of the window, whose framebuffer size stays the last one.
    void close() noexcept {
        handle.store(nullptr);
    }

    [[nodiscard]] ANativeWindow* getNative() const noexcept {
        return handle.load();
    }

    [[nodiscard]] int getFramebufferWidth() const noexcept {
        return framebufferWidth.load();
    }

    [[nodiscard]] int getFramebufferHeight() const noexcept {
        return framebufferHeight.load();
    }

    // The native window takes buffers of the size of the framebuffer, which the system stretches over the window, so the GPU device draws them at that size as the whole window.
    [[nodiscard]] gpu::Surface::Size getSurfaceSize() const noexcept {
        const int width = framebufferWidth.load();
        const int height = framebufferHeight.load();
        return {.width = width, .height = height, .windowWidth = width, .windowHeight = height};
    }

  protected:
    [[nodiscard]] bool queryOpen() const noexcept override {
        return running.load();
    }
    [[nodiscard]] void* queryHandle() const noexcept override {
        return handle.load();
    }
    [[nodiscard]] void* queryDisplay() const noexcept override {
        return nullptr;
    }
    [[nodiscard]] void* queryActivity() const noexcept override {
        return &native;
    }
    [[nodiscard]] math::Vec2 queryFramebufferSize() const noexcept override;
    [[nodiscard]] float queryScale() const noexcept override {
        return scale.load();
    }

  private:
    GameActivity& native;
    float maxPixelDensity = 0.0F;
    int windowWidth = 0;
    int windowHeight = 0;
    bool geometrySet = false;
    std::atomic<bool> running = false;
    std::atomic<ANativeWindow*> handle = nullptr;
    std::atomic<int> framebufferWidth = 0;
    std::atomic<int> framebufferHeight = 0;
    std::atomic<float> scale = 1.0F;
};

} // namespace haylen::platform
