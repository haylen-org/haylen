#pragma once

#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "haylen/gpu/Device.hpp"
#include "haylen/gpu/Surface.hpp"
#include "platform/android/AndroidEglContext.hpp"
#include "platform/android/AndroidWindow.hpp"

namespace haylen::platform {

// The graphics of an Android activity. It makes the GPU device of each app through the first graphics API of the order that starts on the device, and hands the device the window of the activity as it comes, changes its size and goes. Vulkan draws into the `ANativeWindow` itself, and OpenGL ES through an EGL context that lasts while the activity lives, whose window surface comes and goes with the window. A graphics API that failed to start is not tried again while the activity lives. It runs on the frame thread.
class AndroidGraphics final {
  public:
    // The graphics APIs to try in turn, and how the window shows the frames.
    struct Settings {
        std::vector<gpu::Device::Api> apis;
        int sampleCount = 1;
        bool vsync = true;
    };

    explicit AndroidGraphics(const AndroidWindow& activityWindow) noexcept : window(activityWindow) {}

    void configure(Settings value) {
        settings = std::move(value);
    }

    // Creates the GPU device of an app, which draws into the window while the activity has one. Throws `std::runtime_error` when no graphics API of the build starts.
    [[nodiscard]] std::unique_ptr<gpu::Device> createDevice();

    // Hand the window to the device of the running app, or to none, when the window comes or changes its size, and take it away before it goes.
    void attachWindow(gpu::Device* device);
    void resizeWindow(gpu::Device* device);
    void detachWindow(gpu::Device* device) noexcept;

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
    [[nodiscard]] bool hasFailed(gpu::Device::Api api) const noexcept;

    const AndroidWindow& window;
    Settings settings;
    std::unique_ptr<AndroidEglContext> egl;
    std::vector<gpu::Device::Api> failed;
};

} // namespace haylen::platform
