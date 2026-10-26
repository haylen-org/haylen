#include "TestPortRuntime.hpp"

#include <exception>
#include <utility>

#include "TestPortHost.hpp"
#include "gpu/DeviceFactory.hpp"
#include "haylen/core/Log.hpp"
#include "platform/Player.hpp"

namespace haylen::platform {

int TestPortRuntime::run(int argc, char* argv[]) {
    TestPortRuntime runtime;
    Player::create(argc, argv, std::make_unique<TestPortHost>(runtime));
    NativeWindow::install(&runtime.window);
    const int status = runtime.play();
    NativeWindow::install(nullptr);
    return status;
}

std::unique_ptr<gpu::Device> TestPortRuntime::createDevice() {
    if (openedDevice != nullptr) {
        return std::move(openedDevice);
    }
    return gpu::DeviceFactory::create(api, gpu::Surface{.kind = gpu::Surface::Kind::Port, .width = static_cast<int>(TestPortWindow::kSize.x), .height = static_cast<int>(TestPortWindow::kSize.y)}, {});
}

// The graphics APIs of the app open the window in turn, as on every platform, and the reason of each one that fails reaches the log.
bool TestPortRuntime::openWindow() {
    for (const gpu::Device::Api candidate : Player::getGraphicsApis()) {
        try {
            api = candidate;
            openedDevice = createDevice();
            window.setOpen(true);
            core::Log::info("The app draws through \"{}\".", gpu::DeviceFactory::getApiName(api));
            return true;
        } catch (const std::exception& error) {
            core::Log::warning("The graphics API \"{}\" could not start. {}", gpu::DeviceFactory::getApiName(candidate), error.what());
        }
    }
    core::Log::error("No graphics API could start on the test port.");
    return false;
}

// The app plays one frame at a time until it quits, and an app that never quits fails the run.
int TestPortRuntime::play() {
    if (!openWindow()) {
        return 1;
    }
    Player::initialize();

    int frames = 0;
    while (!closing && frames < kMaxFrames) {
        if (closeRequested) {
            Player::handleEvent({.type = Event::Type::QuitRequested});
            closing = true;
            break;
        }
        Player::frame(kFrameSeconds);
        ++frames;
    }

    Player::cleanup();
    window.setOpen(false);
    if (!closing) {
        core::Log::error("The app did not quit within {} frames.", kMaxFrames);
        return 1;
    }
    return Player::getExitStatus();
}

} // namespace haylen::platform

int main(int argc, char* argv[]) {
    return haylen::platform::TestPortRuntime::run(argc, argv);
}
