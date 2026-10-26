#pragma once

#include <android/native_window.h>
#include <game-activity/GameActivity.h>

#include <atomic>

#include "platform/NativeWindow.hpp"

namespace haylen::platform {

// The window of an Android activity: its `GameActivity` and the `ANativeWindow` of its surface while one exists, with the size of the framebuffer. An app that turns high DPI off draws into a framebuffer of half the pixels of the window, which the system scales up. The frame thread changes it, and services on other threads read it.
class AndroidWindow final : public NativeWindow {
  public:
    explicit AndroidWindow(GameActivity& activity) noexcept : native(activity) {}

    // Whether the framebuffer takes every pixel of the window, as the app that the activity opens for asks, before the first window opens.
    void setHighDpi(bool value) noexcept {
        fullDensity = value;
    }

    void setRunning(bool value) noexcept {
        running.store(value);
    }

    // Takes the window of the surface, new or of a new size in pixels of the window, and sizes its buffers as the framebuffer.
    void open(ANativeWindow& window, int width, int height) noexcept;

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
    bool fullDensity = true;
    std::atomic<bool> running = false;
    std::atomic<ANativeWindow*> handle = nullptr;
    std::atomic<int> framebufferWidth = 0;
    std::atomic<int> framebufferHeight = 0;
    std::atomic<float> scale = 1.0F;
};

} // namespace haylen::platform
