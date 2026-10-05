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
#include "haylen/lua/Application.hpp"
#include "platform/headless/HeadlessHost.hpp"
#include "support/TemporaryDirectory.hpp"

namespace haylen::test {

// A running engine on the headless host with an in-memory package.
class EngineFixture final {
  public:
    // A fixture in development plays its apps the way the player does with "--dev", so changes queued in the development session of the host reach them.
    struct Options {
        bool development = false;
    };

    explicit EngineFixture(std::map<std::string, std::string> files = {}, std::unique_ptr<core::Application> application = nullptr);
    EngineFixture(std::map<std::string, std::string> files, std::unique_ptr<core::Application> application, Options options);
    ~EngineFixture();

    [[nodiscard]] core::Engine& engine() noexcept {
        return *runningEngine;
    }
    [[nodiscard]] platform::HeadlessHost& host() noexcept {
        return headlessHost;
    }
    [[nodiscard]] io::MemoryPackage& package() noexcept {
        return *memoryPackage;
    }
    [[nodiscard]] lua_State* lua() noexcept {
        return runningEngine->getLuaState();
    }

    // Replaces the engine with a new one that runs the package on the same host with a Lua application, the way the runtime restarts an app, so what the host keeps outlives the app.
    void restart();

    void frames(int count, double seconds = 1.0 / 60.0);
    bool frameUntil(const std::function<bool()>& condition, std::chrono::milliseconds timeout = std::chrono::seconds(10));

    // Runs Lua and returns its first result converted to a string, or the error message prefixed with `error: `.
    std::string lua(const std::string& source);
    void runLua(const std::string& source);

  private:
    void launch(std::unique_ptr<core::Application> application);

    TemporaryDirectory directory;
    platform::HeadlessHost headlessHost;
    std::shared_ptr<io::MemoryPackage> memoryPackage;
    std::unique_ptr<core::Engine> runningEngine;
};

} // namespace haylen::test
