#pragma once

#include <cstddef>
#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include "haylen/2d/graphics/Renderer.hpp"
#include "haylen/core/Connection.hpp"
#include "haylen/core/Log.hpp"
#include "haylen/debug/LogLine.hpp"
#include "haylen/debug/Monitor.hpp"
#include "haylen/debug/Stats.hpp"
#include "haylen/debug/StatsDisplay.hpp"
#include "haylen/input/Key.hpp"
#include "haylen/plugins/Plugin.hpp"

namespace haylen::plugins {

// Shows the debug statistics and the debug drawings and exposes them to Lua as `haylen.debug`. Compact mode draws the main numbers in a corner with the renderer, and full mode shows the overlay window with frame times, profiler scopes, the scenes, the drawings, rendering, memory, GPU pools, counters, object counts, signals and event listeners, monitors and recent log lines. A key cycles through `off`, `compact` and `full`, F3 unless the app picks another, and `app.json` picks the mode the app starts with. Drawings draw over every canvas the app draws, by name: `physics` shows the bodies, shapes, joints and contacts of every physics world, `bounds` outlines the sprites and text blocks and names each sprite after its texture, and drawers that apps and plugins add show anything else. Another key, F4 unless the app picks another, turns every drawing on or off, and `app.json` names the drawings the app starts with.
class DebugPlugin final : public Plugin {
  public:
    static constexpr std::size_t kLogLines = 200;
    static constexpr std::string_view kBoundsDrawing = "bounds";
    static constexpr float kBoundsLabelSize = 12.0F;
    static constexpr math::Color kBoundsColor = math::Color{1.0F, 0.62F, 0.18F, 1.0F};

    // Draws a drawing into the open canvas, whose kind the renderer tells.
    using Drawer = std::function<void(graphics2d::Renderer& renderer)>;

    [[nodiscard]] std::string_view getName() const noexcept override {
        return "debug";
    }
    void start(core::Engine& engine) override;
    void stop(core::Engine& engine) override;
    void event(core::Engine& engine, const platform::Event& event) override;
    void renderUi(core::Engine& engine, const core::SceneView& view) override;
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

    void setDrawing(std::string_view name, bool enabled);
    [[nodiscard]] bool isDrawing(std::string_view name) const;

    // The names of the drawings that are on, and of every drawing with a drawer, each in order of name.
    [[nodiscard]] std::vector<std::string> getDrawings() const;
    [[nodiscard]] std::vector<std::string> getDrawingNames() const;

    // Adds a drawer to a drawing by name, which several drawers may share, and disconnecting the connection removes it.
    core::Connection addDrawer(std::string name, Drawer drawer);

    void setDrawKey(std::optional<input::Key> value) noexcept {
        drawKey = value;
    }
    [[nodiscard]] std::optional<input::Key> getDrawKey() const noexcept {
        return drawKey;
    }

    // Publishes `objectCreated` and `objectDestroyed` on the event bus, queued for the end of the frame, for every object the statistics count. It costs time with many objects, so it starts off.
    void setObjectEvents(bool value);
    [[nodiscard]] bool hasObjectEvents() const noexcept {
        return objectEvents;
    }

    // Takes a snapshot of the engine statistics, with the rendering of the last frame the renderer finished.
    [[nodiscard]] debug::Stats captureStats(core::Engine& engine) const;

    // Adds a monitor sampled once per frame, replacing the monitor with the same name. Disconnecting the connection removes the monitor, and blocking it holds the value and history without sampling.
    core::Connection addMonitor(std::string name, debug::Monitor::Sampler sampler);
    bool removeMonitor(std::string_view name);
    [[nodiscard]] std::vector<std::shared_ptr<debug::Monitor>> getMonitors() const;

    // The latest printed lines, oldest first, from any thread that logged.
    [[nodiscard]] std::vector<debug::LogLine> getRecentLog() const;

  private:
    // A registered monitor and the link of its connection, which ends once the monitor is removed or replaced.
    struct MonitorEntry final : core::Connection::Link {
        DebugPlugin* plugin = nullptr;
        std::shared_ptr<debug::Monitor> monitor;
        bool blocked = false;

        void disconnect() override;
        [[nodiscard]] bool isConnected() const noexcept override {
            return plugin != nullptr;
        }
        void setBlocked(bool value) override {
            blocked = value;
        }
        [[nodiscard]] bool isBlocked() const noexcept override {
            return blocked;
        }
    };

    // A drawer and the link of its connection, which ends once the drawer is removed.
    struct DrawerEntry final : core::Connection::Link {
        DebugPlugin* plugin = nullptr;
        std::string name;
        Drawer drawer;
        bool blocked = false;

        void disconnect() override;
        [[nodiscard]] bool isConnected() const noexcept override {
            return plugin != nullptr;
        }
        void setBlocked(bool value) override {
            blocked = value;
        }
        [[nodiscard]] bool isBlocked() const noexcept override {
            return blocked;
        }
    };

    void record(core::Log::Level level, std::string_view line);

    // Draws the drawings that are on into the canvas that is about to close.
    void drawOverlay(core::Engine& engine, graphics2d::Renderer& renderer);
    void toggleDrawings();

    // Removes the entries that match, ending their connections.
    bool removeMonitors(const std::function<bool(const MonitorEntry&)>& matches);

    mutable std::mutex linesMutex;
    std::deque<debug::LogLine> lines;
    std::vector<std::shared_ptr<MonitorEntry>> monitors;
    std::vector<std::shared_ptr<DrawerEntry>> drawers;
    std::set<std::string, std::less<>> drawings;
    core::Engine* owner = nullptr;
    graphics2d::Renderer::Stats lastRendering;
    std::uint64_t listener = 0;
    std::uint64_t overlay = 0;
    std::optional<input::Key> toggleKey = input::Key::F3;
    std::optional<input::Key> drawKey = input::Key::F4;
    debug::StatsDisplay::Mode statsMode = debug::StatsDisplay::Mode::Off;
    bool objectEvents = false;
};

} // namespace haylen::plugins
