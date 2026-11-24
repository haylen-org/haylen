#pragma once

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

#include "haylen/gpu/Device.hpp"

namespace haylen::platform {

// Opens the window of a platform layer and starts its GPU device through the graphics APIs of the app in order. Each API opens the window with what it draws through and starts its device, and the first that starts keeps the window. The reasons of the ones that failed reach the log, so a machine that falls back says why.
class GraphicsApiFallback final {
  public:
    // The window of a platform layer as the fallback drives it, for one graphics API at a time.
    class Window {
      public:
        virtual ~Window() = default;

        // Whether the graphics API is ready to start a device, which some APIs, such as WebGPU, report only in a later frame.
        [[nodiscard]] virtual bool prepare(gpu::Device::Api api) = 0;

        // The graphics API the window is open for, or `None` while it is closed.
        [[nodiscard]] virtual gpu::Device::Api getApi() const noexcept = 0;

        // Opens the window for the graphics API. Throws `std::runtime_error` when the system cannot make such a window.
        virtual void open(gpu::Device::Api api) = 0;

        // Reads the size of the window again and tells whether it has an area to draw into.
        [[nodiscard]] virtual bool hasArea() = 0;

        // Starts the device of the graphics API on the window. Throws `std::runtime_error` when the device cannot start.
        [[nodiscard]] virtual std::unique_ptr<gpu::Device> createDevice(gpu::Device::Api api) = 0;

        virtual void close() noexcept = 0;
    };

    // Takes the graphics APIs in the order to try them, and the sentence that the app fails with when none starts.
    GraphicsApiFallback(std::vector<gpu::Device::Api> order, std::string failure);

    // Returns the device of the first graphics API that starts on the window, or nothing while an API prepares or the window has no area, so the platform layer asks again in a later frame from the same API. Throws `std::runtime_error` with the sentence of the failure once no API is left.
    [[nodiscard]] std::unique_ptr<gpu::Device> open(Window& window);

  private:
    std::vector<gpu::Device::Api> apis;
    std::size_t next = 0;
    std::string failure;
};

} // namespace haylen::platform
