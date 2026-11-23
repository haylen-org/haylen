#pragma once

#include <array>
#include <span>
#include <string_view>

#include "haylen/core/Application.hpp"

namespace haylen::lua {

// Runs `source/main.lua` from the app package, after loading the autoloads that the `lua` section of `haylen.json` lists. Apps written in Lua use it as their application, which reads the `lua` section and adds the script plugin and the development tools of Lua after the plugins of the engine.
class Application final : public haylen::core::Application {
  public:
    // The sections of `haylen.json` that the Lua layer reads, for the tools that read the `haylen.json` of a Lua app without running it.
    static const std::array<std::string_view, 1> kSections;

    [[nodiscard]] std::span<const std::string_view> getSections() const override;

    // Validates the `lua` section, so a wrong value fails the app with its reason before the engine starts.
    void configure(haylen::core::AppConfig& config) override;
    void addPlugins(haylen::core::Engine& engine) override;
    void start(haylen::core::Engine& engine) override;

    // Stops the autoloads, the last one first, while every other system still runs.
    void stop(haylen::core::Engine& engine) override;
};

} // namespace haylen::lua
