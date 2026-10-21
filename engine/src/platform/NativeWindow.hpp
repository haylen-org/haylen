#pragma once

#include <atomic>

#include "haylen/math/Vec2.hpp"

namespace haylen::platform {

// The window of the backend as the system of each platform sees it, which the services of a platform read to place native dialogs, text fields, views, menus and accessibility over the app. The backend installs its window before the system creates it and removes it once it closed, so the services that the system calls while it creates the window find it, and services on other threads read what is open.
class NativeWindow {
  public:
    NativeWindow() = default;
    NativeWindow(const NativeWindow&) = delete;
    NativeWindow& operator=(const NativeWindow&) = delete;
    virtual ~NativeWindow() = default;

    // Whether the system created the window and the backend runs it.
    [[nodiscard]] static bool isOpen() noexcept {
        const NativeWindow* window = current.load();
        return window != nullptr && window->queryOpen();
    }

    // The handle of the window in the system: an `NSWindow` on macOS, a `UIWindow` on iOS, iPadOS, Mac Catalyst and tvOS, an `HWND` on Windows, an X11 `Window` on Linux and an `ANativeWindow` on Android, or null while no window is open and where the platform has none, such as the web.
    [[nodiscard]] static void* getHandle() noexcept {
        const NativeWindow* window = current.load();
        return window != nullptr ? window->queryHandle() : nullptr;
    }

    // The X11 `Display` of the window on Linux, and null elsewhere or while no window is open.
    [[nodiscard]] static void* getDisplay() noexcept {
        const NativeWindow* window = current.load();
        return window != nullptr ? window->queryDisplay() : nullptr;
    }

    // The `GameActivity` that holds the window on Android, and null elsewhere or while no window is open.
    [[nodiscard]] static void* getActivity() noexcept {
        const NativeWindow* window = current.load();
        return window != nullptr ? window->queryActivity() : nullptr;
    }

    // The size of the framebuffer in pixels and the framebuffer pixels per unit of the coordinates the system gives the window, points on Apple platforms and the web, and pixels of the window on Android, which an installed window reports from the moment the system creates it.
    [[nodiscard]] static math::Vec2 getFramebufferSize() noexcept {
        return current.load()->queryFramebufferSize();
    }

    [[nodiscard]] static float getScale() noexcept {
        return current.load()->queryScale();
    }

    static void install(NativeWindow* window) noexcept {
        current.store(window);
    }

  protected:
    [[nodiscard]] virtual bool queryOpen() const noexcept = 0;
    [[nodiscard]] virtual void* queryHandle() const noexcept = 0;
    [[nodiscard]] virtual void* queryDisplay() const noexcept = 0;
    [[nodiscard]] virtual void* queryActivity() const noexcept = 0;
    [[nodiscard]] virtual math::Vec2 queryFramebufferSize() const noexcept = 0;
    [[nodiscard]] virtual float queryScale() const noexcept = 0;

  private:
    static inline std::atomic<NativeWindow*> current = nullptr;
};

} // namespace haylen::platform
