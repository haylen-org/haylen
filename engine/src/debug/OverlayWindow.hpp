#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "haylen/core/EventBus.hpp"
#include "haylen/core/Log.hpp"
#include "haylen/debug/LogLine.hpp"
#include "haylen/debug/Monitor.hpp"
#include "haylen/debug/Stats.hpp"

struct ImVec4;

namespace haylen::content {
class ContentManager;
}

namespace haylen::core {
class Engine;
class SceneManager;
} // namespace haylen::core

namespace haylen::plugins {
class DebugPlugin;
}

namespace haylen::ui {
class Backend;
}

namespace haylen::debug {

class Profiler;

// Draws the full debug overlay window with ImGui: frame times, profiler scopes, the scenes on the stack, the debug drawings, rendering, memory, GPU pools, counters, the reads and downloads of the content, object counts, signals and event listeners, monitors and the recent log. It reads what it shows into storage it keeps and writes its numbers into fixed buffers, so a frame that shows what the frame before it showed allocates nothing.
class OverlayWindow final {
  public:
    static constexpr float kWidth = 720.0F;
    static constexpr float kHeight = 860.0F;
    static constexpr std::string_view kDrawingsHint = "Each drawing that is on draws over every canvas of the app.";

    // Draws the window and returns `false` once the player closed it.
    bool draw(core::Engine& engine, ui::Backend& backend, plugins::DebugPlugin& plugin);

  private:
    // A text of a number of bytes, such as `1.5 MB`, which every count of bytes fits.
    using ByteText = std::array<char, 32>;

    // A label of a row, such as the name of a pool or a monitor with its value, cut to the room it has.
    using Label = std::array<char, 160>;

    [[nodiscard]] static ImVec4 getLevelColor(core::Log::Level level) noexcept;
    static const char* formatBytes(ByteText& text, std::int64_t bytes) noexcept;
    [[nodiscard]] static std::size_t countStale(const std::vector<core::EventBus::Topic>& topics) noexcept;

    void drawFrame(const Stats::Frame& frame);
    static void drawEventLoop(const Stats::EventLoop& loop);
    static void drawProfiler(const Profiler& profiler);
    static void drawScenes(const core::SceneManager& scenes);
    void drawDrawings(plugins::DebugPlugin& plugin);
    static void drawRendering(const graphics2d::Renderer::Stats& rendering);
    void drawResources();
    static void drawContent(const content::ContentManager& content);
    static void drawObjects(const std::vector<ObjectCounter::Snapshot>& objects);
    static void drawTopics(const char* id, const std::vector<core::EventBus::Topic>& topics);
    void drawMonitors();
    void drawLog();

    Stats stats;
    std::vector<float> history;
    std::vector<std::shared_ptr<Monitor>> monitors;
    std::vector<LogLine> log;
    std::vector<core::EventBus::Topic> events;
    std::vector<core::EventBus::Topic> signals;
    std::vector<std::string> drawingNames;
    std::string voices;
};

} // namespace haylen::debug
