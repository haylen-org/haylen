#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "haylen/core/EventBus.hpp"
#include "haylen/core/Log.hpp"
#include "haylen/debug/LogLine.hpp"
#include "haylen/debug/Monitor.hpp"
#include "haylen/debug/Stats.hpp"

struct ImVec4;

namespace haylen::core {
class Engine;
}

namespace haylen::debug {

class Profiler;

// Draws the full debug overlay window with ImGui: frame times, profiler scopes, rendering, memory, GPU pools, counters, object counts, signals and event listeners, monitors and the recent log.
class OverlayWindow final {
  public:
    // Draws the window and returns false once the player closed it.
    static bool draw(core::Engine& engine, const Stats& stats, const std::vector<std::shared_ptr<Monitor>>& monitors, const std::vector<LogLine>& log);

  private:
    [[nodiscard]] static ImVec4 getLevelColor(core::Log::Level level) noexcept;
    [[nodiscard]] static std::string formatBytes(std::int64_t bytes);
    [[nodiscard]] static std::size_t countStale(const std::vector<core::EventBus::Topic>& topics) noexcept;

    static void drawFrame(const Profiler& profiler, const Stats::Frame& frame);
    static void drawProfiler(const Profiler& profiler);
    static void drawRendering(const graphics2d::Renderer::Stats& rendering);
    static void drawResources(const Stats& stats);
    static void drawObjects(const std::vector<ObjectCounter::Snapshot>& objects);
    static void drawTopics(const char* id, const std::vector<core::EventBus::Topic>& topics);
    static void drawMonitors(const std::vector<std::shared_ptr<Monitor>>& monitors);
    static void drawLog(const std::vector<LogLine>& log);
};

} // namespace haylen::debug
