#include "haylen/plugins/DebugPlugin.hpp"

#include <imgui.h>

#include <algorithm>
#include <utility>

#include "debug/DebugLua.hpp"
#include "debug/OverlayWindow.hpp"
#include "haylen/core/Engine.hpp"
#include "haylen/core/EventBus.hpp"
#include "haylen/core/LifecycleEvent.hpp"
#include "haylen/debug/ObjectCounter.hpp"
#include "haylen/debug/Profiler.hpp"
#include "haylen/platform/Event.hpp"
#include "haylen/plugins/UiPlugin.hpp"

namespace haylen::plugins {

void DebugPlugin::start(core::Engine& engine) {
    owner = &engine;
    listener = core::Log::addListener([this](core::Log::Level level, std::string_view line) { record(level, line); });
    statsMode = engine.getConfig().debug.stats;
    setObjectEvents(engine.getConfig().debug.objectEvents);
}

void DebugPlugin::stop(core::Engine&) {
    setObjectEvents(false);
    core::Log::removeListener(listener);
    monitors.clear();
    owner = nullptr;
}

void DebugPlugin::record(core::Log::Level level, std::string_view line) {
    const std::scoped_lock lock(linesMutex);
    lines.push_back({.level = level, .text = std::string(line)});
    while (lines.size() > kLogLines) {
        lines.pop_front();
    }
}

std::vector<debug::LogLine> DebugPlugin::getRecentLog() const {
    const std::scoped_lock lock(linesMutex);
    return {lines.begin(), lines.end()};
}

// The observer is global to the process, so only a running plugin installs it, and events go through the queue because objects come and go on any thread.
void DebugPlugin::setObjectEvents(bool value) {
    objectEvents = value && owner != nullptr;
    if (!objectEvents) {
        debug::ObjectCounter::setObserver({});
        return;
    }
    core::EventBus* events = &owner->getEvents();
    // clang-format off
    debug::ObjectCounter::setObserver([events](std::string_view type, bool created, std::size_t count) {
        events->post(std::string(created ? core::LifecycleEvent::kObjectCreated : core::LifecycleEvent::kObjectDestroyed), {{"type", std::string(type)}, {"count", count}});
    });
    // clang-format on
}

debug::Stats DebugPlugin::captureStats(core::Engine& engine) const {
    return debug::Stats::capture(engine, lastRendering);
}

void DebugPlugin::addMonitor(std::string name, debug::Monitor::Sampler sampler) {
    auto monitor = std::make_shared<debug::Monitor>(std::move(name), std::move(sampler));
    removeMonitor(monitor->getName());
    monitors.push_back(std::move(monitor));
}

bool DebugPlugin::removeMonitor(std::string_view name) {
    return std::erase_if(monitors, [name](const std::shared_ptr<debug::Monitor>& monitor) { return monitor->getName() == name; }) > 0;
}

void DebugPlugin::event(core::Engine&, const platform::Event& event) {
    if (event.type != platform::Event::Type::KeyDown || event.repeat || !toggleKey || event.key != *toggleKey) {
        return;
    }
    switch (statsMode) {
    case debug::StatsDisplay::Mode::Off:
        statsMode = debug::StatsDisplay::Mode::Compact;
        break;
    case debug::StatsDisplay::Mode::Compact:
        statsMode = debug::StatsDisplay::Mode::Full;
        break;
    case debug::StatsDisplay::Mode::Full:
        statsMode = debug::StatsDisplay::Mode::Off;
        break;
    }
}

void DebugPlugin::renderUi(core::Engine& engine) {
    if (statsMode != debug::StatsDisplay::Mode::Full) {
        return;
    }
    ui::Backend& backend = engine.getPlugin<UiPlugin>().getBackend();
    backend.makeCurrent();

    const math::Rect safe = backend.getSafeRect();
    ImGui::SetNextWindowPos({safe.x + 16.0F, safe.y + 16.0F}, ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize({720.0F, std::min(860.0F, safe.height - 32.0F)}, ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowBgAlpha(0.9F);
    if (!debug::OverlayWindow::draw(engine, captureStats(engine), monitors, getRecentLog())) {
        statsMode = debug::StatsDisplay::Mode::Off;
    }
}

void DebugPlugin::renderOverlay(core::Engine& engine) {
    if (statsMode != debug::StatsDisplay::Mode::Compact) {
        return;
    }
    const debug::Profiler& profiler = engine.getProfiler();
    const double average = profiler.getAverageFrameMilliseconds();
    debug::StatsDisplay::drawCompact(engine, {.fps = average > 0.0 ? 1000.0 / average : 0.0, .milliseconds = profiler.getLastFrameMilliseconds(), .drawCalls = lastRendering.drawCalls, .vertices = lastRendering.vertices, .instances = lastRendering.instances});
}

// The renderer has just submitted the frame, so its statistics are complete, and monitors sample what the frame left behind. A monitor may add or remove monitors while it samples, so the loop walks a copy.
void DebugPlugin::endFrame(core::Engine& engine) {
    lastRendering = engine.getRenderer2D().getStats();
    const std::vector<std::shared_ptr<debug::Monitor>> snapshot = monitors;
    for (const std::shared_ptr<debug::Monitor>& monitor : snapshot) {
        monitor->sample();
    }
}

void DebugPlugin::installLua(core::Engine&, lua_State* L) {
    debug::DebugLua::install(L);
}

} // namespace haylen::plugins
