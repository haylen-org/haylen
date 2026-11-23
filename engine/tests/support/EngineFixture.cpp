#include "support/EngineFixture.hpp"

#include <stdexcept>

#include "haylen/io/MemoryPackage.hpp"
#include "haylen/platform/DevelopmentSession.hpp"
#include "io/OverlayPackage.hpp"
#include "support/EventLoop.hpp"
#include "support/TestApplication.hpp"
#include "support/TestFiles.hpp"

namespace haylen::test {

EngineFixture::EngineFixture(std::map<std::string, std::string> files, std::unique_ptr<core::Application> application) : EngineFixture(std::move(files), std::move(application), Options{}) {}

// The default application of the core fixture runs no code, so a test drives the engine from C++.
EngineFixture::EngineFixture(std::map<std::string, std::string> files, std::unique_ptr<core::Application> application, Options options) : EngineFixture(std::move(files), std::move(application), std::move(options), [] { return std::make_unique<TestApplication>(nullptr); }) {}

EngineFixture::EngineFixture(std::map<std::string, std::string> files, std::unique_ptr<core::Application> application, Options options, ApplicationMaker maker) : headlessHost(directory.getPath() / "data"), makeApplication(std::move(maker)) {
    if (options.development || !options.developmentServer.empty()) {
        headlessHost.enableDevelopment(std::nullopt, options.developmentServer);
    }

    headlessHost.setAudioOutput(options.audioOutput);
    if (options.gpuDevice) {
        headlessHost.setGpuDeviceMaker(std::move(options.gpuDevice));
    }

    files.try_emplace("haylen.json", R"({"name": "Test App", "identifier": "com.haylen.tests"})");

    std::map<std::string, std::vector<std::uint8_t>> contents;
    for (const auto& [path, text] : files) {
        contents.emplace(path, TestFiles::bytes(text));
    }

    memoryPackage = std::make_shared<io::MemoryPackage>("test", std::move(contents));
    playedPackage = memoryPackage;
    if (platform::DevelopmentSession* session = headlessHost.getDevelopmentSession(); session != nullptr && session->isConnected()) {
        auto overlay = std::make_shared<io::OverlayPackage>(memoryPackage);
        session->setOverlay(overlay);
        playedPackage = overlay;
    }

    launch(std::move(application));
}

EngineFixture::~EngineFixture() {
    stop();
}

void EngineFixture::stop() {
    runningEngine.reset();
}

void EngineFixture::launch(std::unique_ptr<core::Application> application) {
    if (!application) {
        application = makeApplication();
    }

    core::AppConfig config = core::AppConfig::fromPackage(*playedPackage, application->getSections());

    application->configure(config);
    headlessHost.setTransparencySupported(config.window.transparent);
    headlessHost.setWindowSampleCount(config.window.sampleCount);
    runningEngine = std::make_unique<core::Engine>(headlessHost, playedPackage, std::move(config), std::move(application));
    runningEngine->start();
}

platform::DevelopmentSession& EngineFixture::developmentSession() {
    platform::DevelopmentSession* session = headlessHost.getDevelopmentSession();
    if (session == nullptr) {
        throw std::logic_error("The fixture runs outside development, so it has no development session. Give it the option \"development\".");
    }

    return *session;
}

void EngineFixture::restart() {
    stop();
    launch(nullptr);
}

void EngineFixture::frames(int count, double seconds) {
    for (int frame = 0; frame < count; ++frame) {
        runningEngine->frame(seconds);
    }
}

bool EngineFixture::frameUntil(const std::function<bool()>& condition, std::chrono::milliseconds timeout) {
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while (!condition()) {
        if (std::chrono::steady_clock::now() > deadline) {
            return false;
        }

        runningEngine->frame(1.0 / 60.0);
        EventLoop::yield();
    }

    return true;
}

} // namespace haylen::test
