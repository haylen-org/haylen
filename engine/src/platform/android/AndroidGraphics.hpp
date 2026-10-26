#pragma once

#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "haylen/gpu/Device.hpp"
#include "haylen/gpu/Surface.hpp"
#include "platform/android/AndroidEglContext.hpp"
#include "platform/android/AndroidGraphicsApis.hpp"
#include "platform/android/AndroidWindow.hpp"

namespace haylen::platform {

// The graphics of an Android activity. It makes the GPU device of each app through the first graphics API of the order that starts on the device, as `AndroidGraphicsApis` keeps it, and hands the device the window of the activity as it comes, changes its size and goes. Vulkan draws into the `ANativeWindow` itself, and OpenGL ES through an EGL context that lasts while the activity lives, whose window surface comes and goes with the window, until the GPU loses it. It runs on the frame thread.
class AndroidGraphics final {
  public:
    // How the window shows the frames.
    struct Settings {
        int sampleCount = 1;
        bool vsync = true;
    };

    explicit AndroidGraphics(const AndroidWindow& activityWindow) noexcept : window(activityWindow) {}

    // Takes the graphics APIs to try in turn and the settings of the window, before the first device.
    void configure(std::vector<gpu::Device::Api> order, Settings value) {
        apis.setOrder(std::move(order));
        settings = value;
    }

    // Creates the GPU device of an app, which draws into the window while the activity has one. Throws `std::runtime_error` when no graphics API of the build starts.
    [[nodiscard]] std::unique_ptr<gpu::Device> createDevice();

    // Hand the window to the device of the running app, or to none, when the window comes or changes its size, and take it away before it goes.
    void attachWindow(gpu::Device* device);
    void resizeWindow(gpu::Device* device);
    void detachWindow(gpu::Device* device) noexcept;

    // Whether the device stopped working, as it reports itself or as the EGL context it draws through reports.
    [[nodiscard]] bool isLost(const gpu::Device& device) const noexcept;

    // Records that the device of the API stopped working, so the next one starts on a new context.
    void loseDevice(gpu::Device::Api api) noexcept {
        apis.lose(api);
    }

    // Ends the EGL context once the activity ends, after the last device.
    void release() noexcept {
        egl.reset();
    }

  private:
#if defined(NDEBUG)
    static constexpr bool kValidation = false;
#else
    static constexpr bool kValidation = true;
#endif

    [[nodiscard]] std::unique_ptr<gpu::Device> open(gpu::Device::Api api);
    [[nodiscard]] std::optional<gpu::Surface> describeSurface(gpu::Device::Api api) const;

    const AndroidWindow& window;
    Settings settings;
    AndroidGraphicsApis apis;
    std::unique_ptr<AndroidEglContext> egl;
};

} // namespace haylen::platform
