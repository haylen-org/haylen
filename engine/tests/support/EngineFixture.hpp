#pragma once

#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "haylen/core/AppConfig.hpp"
#include "haylen/core/Application.hpp"
#include "haylen/core/Engine.hpp"
#include "haylen/io/MemoryPackage.hpp"
#include "haylen/io/Package.hpp"
#include "platform/headless/HeadlessHost.hpp"
#include "support/TemporaryDirectory.hpp"

namespace haylen::test {

// A running engine on the headless host with an in-memory package, which runs a C++ application, by default one that does nothing, so the tests of the core run without Lua.
class EngineFixture {
  public:
    // Makes the application of the fixture when a test gives none, at the start and at every restart.
    using ApplicationMaker = std::function<std::unique_ptr<core::Application>()>;

    // A fixture in development plays its apps the way the player does with "--dev", so changes queued in the development session of the host reach them. With the address of a development server, the session connects to it and the package plays under an overlay that takes the files the server sends. An audio output gives the mixer a device that opens through that output, and a maker of GPU devices draws the apps through the devices it makes instead of the null device.
    struct Options {
        bool development = false;
        std::string developmentServer;
        std::shared_ptr<port::AudioOutput> audioOutput;
        std::function<std::unique_ptr<gpu::Device>()> gpuDevice;
    };

    explicit EngineFixture(std::map<std::string, std::string> files = {}, std::unique_ptr<core::Application> application = nullptr);
    EngineFixture(std::map<std::string, std::string> files, std::unique_ptr<core::Application> application, Options options);
    virtual ~EngineFixture();

    EngineFixture(const EngineFixture&) = delete;
    EngineFixture& operator=(const EngineFixture&) = delete;

    [[nodiscard]] core::Engine& engine() noexcept {
        return *runningEngine;
    }
    [[nodiscard]] platform::HeadlessHost& host() noexcept {
        return headlessHost;
    }
    [[nodiscard]] io::MemoryPackage& package() noexcept {
        return *memoryPackage;
    }

    // The development session of the host, which only a fixture in development has, and which throws for a fixture that runs outside development.
    [[nodiscard]] platform::DevelopmentSession& developmentSession();

    // Replaces the engine with a new one that runs the package on the same host with the application of the fixture, the way the runtime restarts an app, so what the host keeps outlives the app.
    void restart();

    void frames(int count, double seconds = 1.0 / 60.0);
    bool frameUntil(const std::function<bool()>& condition, std::chrono::milliseconds timeout = std::chrono::seconds(10));

  protected:
    // A fixture whose application, when a test gives none, is the one that the maker makes, with the files that the package holds by default.
    EngineFixture(std::map<std::string, std::string> files, std::unique_ptr<core::Application> application, Options options, ApplicationMaker maker);

  private:
    void launch(std::unique_ptr<core::Application> application);
    void stop();

    TemporaryDirectory directory;
    platform::HeadlessHost headlessHost;
    std::shared_ptr<io::MemoryPackage> memoryPackage;
    std::shared_ptr<io::Package> playedPackage;
    std::unique_ptr<core::Engine> runningEngine;
    ApplicationMaker makeApplication;
};

} // namespace haylen::test
