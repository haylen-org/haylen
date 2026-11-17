#include "TestPortRuntime.hpp"

#include <exception>
#include <utility>

#include "TestPortHost.hpp"
#include "TestPortServices.hpp"
#include "haylen/core/Log.hpp"
#include "haylen/port/GpuDevices.hpp"
#include "haylen/port/Player.hpp"

namespace haylen::platform {

int TestPortRuntime::run(int argc, char* argv[]) {
    const auto audio = std::make_shared<TestPortAudioOutput>();
    port::Player::installServices(TestPortServices::create(std::filesystem::absolute(argv[0]).parent_path(), audio));
    TestPortRuntime runtime(audio);
    port::Player::create(argc, argv, std::make_unique<TestPortHost>(runtime));
    return runtime.play();
}

std::unique_ptr<gpu::Device> TestPortRuntime::createDevice() {
    if (openedDevice != nullptr) {
        return std::move(openedDevice);
    }

    return port::GpuDevices::create(api, gpu::Surface{.kind = gpu::Surface::Kind::Port, .size = window.getSurfaceSize()}, {});
}

void TestPortRuntime::setMaxPixelDensity(float cap) {
    window.setMaxPixelDensity(cap);
    if (gpu::Device* device = port::Player::getGpuDevice()) {
        device->resizeSurface(window.getSurfaceSize());
    }
}

// The graphics APIs of the app open the window in turn, as on every platform, and the reason of each one that fails reaches the log.
bool TestPortRuntime::openWindow() {
    for (const gpu::Device::Api candidate : port::Player::getGraphicsApis()) {
        try {
            api = candidate;
            openedDevice = createDevice();
            core::Log::info("The app draws through \"{}\".", port::GpuDevices::getApiName(api));
            return true;
        } catch (const std::exception& error) {
            core::Log::warning("The graphics API \"{}\" could not start. {}", port::GpuDevices::getApiName(candidate), error.what());
        }
    }

    core::Log::error("No graphics API could start on the test port.");
    return false;
}

// The app plays one frame at a time until it quits, and an app that never quits fails the run, as one that plays no sound through the output of the port does.
int TestPortRuntime::play() {
    if (!openWindow()) {
        return 1;
    }

    port::Player::initialize();

    int frames = 0;
    while (!closing && frames < kMaxFrames) {
        if (closeRequested) {
            port::Player::handleEvent({.type = Event::Type::QuitRequested});
            closing = true;
            break;
        }

        port::Player::frame(kFrameSeconds);
        audio->play();
        ++frames;
    }

    port::Player::cleanup();

    if (!closing) {
        core::Log::error("The app did not quit within {} frames.", kMaxFrames);
        return 1;
    }

    if (audio->getPlayedFrames() == 0) {
        core::Log::error("The app played no sound through the audio output of the test port.");
        return 1;
    }

    core::Log::info("The test port played {} frames of sound through its audio output.", audio->getPlayedFrames());
    return port::Player::getExitStatus();
}

} // namespace haylen::platform

int main(int argc, char* argv[]) {
    return haylen::platform::TestPortRuntime::run(argc, argv);
}
