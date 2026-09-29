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
#include "haylen/core/Scene.hpp"
#include "haylen/io/MemoryPackage.hpp"
#include "haylen/io/Package.hpp"
#include "haylen/lua/Application.hpp"
#include "platform/headless/HeadlessHost.hpp"
#include "support/TemporaryDirectory.hpp"

namespace haylen::test {

// Scene that runs a drawing function inside a real engine frame.
class DrawingScene final : public core::Scene {
  public:
    explicit DrawingScene(std::function<void(core::Engine&)> function) : draw(std::move(function)) {}

    void render(core::Engine& engine) override {
        draw(engine);
    }

  private:
    std::function<void(core::Engine&)> draw;
};

// A running engine on the headless host with an in-memory package.
class EngineFixture final {
  public:
    explicit EngineFixture(std::map<std::string, std::string> files = {}, std::unique_ptr<core::Application> application = nullptr);
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

    void frames(int count, double seconds = 1.0 / 60.0);
    bool frameUntil(const std::function<bool()>& condition, std::chrono::milliseconds timeout = std::chrono::seconds(10));

    // Runs Lua and returns its first result converted to a string, or the error message prefixed with "error: ".
    std::string lua(const std::string& source);
    void runLua(const std::string& source);

  private:
    TemporaryDirectory directory;
    platform::HeadlessHost headlessHost;
    std::shared_ptr<io::MemoryPackage> memoryPackage;
    std::unique_ptr<core::Engine> runningEngine;
};

[[nodiscard]] std::vector<std::uint8_t> bytes(const std::string& text);
[[nodiscard]] std::vector<std::uint8_t> pngImage(int width, int height, std::uint32_t rgba);

} // namespace haylen::test
