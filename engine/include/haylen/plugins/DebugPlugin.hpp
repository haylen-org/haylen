#pragma once

#include <cstddef>
#include <cstdint>
#include <deque>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "haylen/2d/graphics/Renderer.hpp"
#include "haylen/core/Log.hpp"
#include "haylen/debug/LogLine.hpp"
#include "haylen/debug/Monitor.hpp"
#include "haylen/debug/Stats.hpp"
#include "haylen/debug/StatsDisplay.hpp"
#include "haylen/input/Key.hpp"
#include "haylen/plugins/Plugin.hpp"

namespace haylen::plugins {

// Shows the debug statistics and exposes them to Lua as haylen.debug. Compact mode draws the main numbers in a corner with the renderer, and full mode shows the overlay window with frame times, profiler scopes, rendering, memory, GPU pools, counters, object counts, signals and event listeners, monitors and recent log lines. A key cycles through off, compact and full, F3 unless the app picks another, and app.json picks the mode the app starts with.
class DebugPlugin final : public Plugin {
  public:
    static constexpr std::size_t kLogLines = 200;

    [[nodiscard]] std::string_view getName() const noexcept override {
        return "debug";
    }
    void start(core::Engine& engine) override;
    void stop(core::Engine& engine) override;
    void event(core::Engine& engine, const platform::Event& event) override;
    void renderUi(core::Engine& engine) override;
    void renderOverlay(core::Engine& engine) override;
    void endFrame(core::Engine& engine) override;
    void installLua(core::Engine& engine, lua_State* L) override;

    void setStatsMode(debug::StatsDisplay::Mode value) noexcept {
        statsMode = value;
    }
    [[nodiscard]] debug::StatsDisplay::Mode getStatsMode() const noexcept {
        return statsMode;
    }
    void setToggleKey(std::optional<input::Key> value) noexcept {
        toggleKey = value;
    }
    [[nodiscard]] std::optional<input::Key> getToggleKey() const noexcept {
        return toggleKey;
    }

    // Publishes object_created and object_destroyed on the event bus, queued for the end of the frame, for every object the statistics count. It costs time with many objects, so it starts off.
    void setObjectEvents(bool value);
    [[nodiscard]] bool hasObjectEvents() const noexcept {
        return objectEvents;
    }

    // Takes a snapshot of the engine statistics, with the rendering of the last frame the renderer finished.
    [[nodiscard]] debug::Stats captureStats(core::Engine& engine) const;

    // Adds a monitor sampled once per frame, replacing the monitor with the same name.
    void addMonitor(std::string name, debug::Monitor::Sampler sampler);
    bool removeMonitor(std::string_view name);
    [[nodiscard]] const std::vector<std::shared_ptr<debug::Monitor>>& getMonitors() const noexcept {
        return monitors;
    }

    // The latest printed lines, oldest first, from any thread that logged.
    [[nodiscard]] std::vector<debug::LogLine> getRecentLog() const;

  private:
    void record(core::Log::Level level, std::string_view line);

    mutable std::mutex linesMutex;
    std::deque<debug::LogLine> lines;
    std::vector<std::shared_ptr<debug::Monitor>> monitors;
    core::Engine* owner = nullptr;
    graphics2d::Renderer::Stats lastRendering;
    std::uint64_t listener = 0;
    std::optional<input::Key> toggleKey = input::Key::F3;
    debug::StatsDisplay::Mode statsMode = debug::StatsDisplay::Mode::Off;
    bool objectEvents = false;
};

} // namespace haylen::plugins
