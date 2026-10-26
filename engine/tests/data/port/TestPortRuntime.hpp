#pragma once

#include <cstddef>
#include <memory>

#include "TestPortWindow.hpp"
#include "haylen/gpu/Device.hpp"

namespace haylen::platform {

// The runtime of the test port, which holds the entry point, plays the app that the command line names on the device of the first graphics API that starts, and ends with the status the app quit with.
class TestPortRuntime final {
  public:
    // The frames an app has to quit within, after which the run fails.
    static constexpr int kMaxFrames = 600;
    static constexpr double kFrameSeconds = 1.0 / 60.0;

    [[nodiscard]] static int run(int argc, char* argv[]);

    // Hands the player the device that the window opened with, and a new device of the same API afterwards.
    [[nodiscard]] std::unique_ptr<gpu::Device> createDevice();

    // Asks the app to quit as a request of the platform does, which closes the window once the app heard it.
    void requestClose() noexcept {
        closeRequested = true;
    }

    void close() noexcept {
        closing = true;
    }

  private:
    [[nodiscard]] bool openWindow();
    [[nodiscard]] int play();

    TestPortWindow window;
    gpu::Device::Api api = gpu::Device::Api::None;
    std::unique_ptr<gpu::Device> openedDevice;
    bool closeRequested = false;
    bool closing = false;
};

} // namespace haylen::platform
