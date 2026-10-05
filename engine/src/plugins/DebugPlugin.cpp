#include "haylen/plugins/DebugPlugin.hpp"

#include <imgui.h>

#include <algorithm>
#include <utility>

#include "debug/DebugLua.hpp"
#include "debug/OverlayWindow.hpp"
#include "haylen/core/Engine.hpp"
#include "haylen/core/EventBus.hpp"
#include "haylen/core/LifecycleEvent.hpp"
#include "haylen/core/SceneView.hpp"
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
    drawings.insert(engine.getConfig().debug.drawings.begin(), engine.getConfig().debug.drawings.end());
    overlay = engine.getRenderer2D().addCanvasOverlay([this, &engine](graphics2d::Renderer& renderer) { drawOverlay(engine, renderer); });
}

void DebugPlugin::stop(core::Engine& engine) {
    engine.getRenderer2D().removeCanvasOverlay(overlay);
    setObjectEvents(false);
    core::Log::removeListener(listener);
    removeMonitors([](const MonitorEntry&) { return true; });
    for (const std::shared_ptr<DrawerEntry>& entry : std::exchange(drawers, {})) {
        entry->plugin = nullptr;
    }
    drawings.clear();
    owner = nullptr;
}

void DebugPlugin::setDrawing(std::string_view name, bool enabled) {
    if (enabled) {
        drawings.emplace(name);
        return;
    }
    if (const auto found = drawings.find(name); found != drawings.end()) {
        drawings.erase(found);
    }
}

bool DebugPlugin::isDrawing(std::string_view name) const {
    return drawings.contains(name);
}

std::vector<std::string> DebugPlugin::getDrawings() const {
    return {drawings.begin(), drawings.end()};
}

std::vector<std::string> DebugPlugin::getDrawingNames() const {
    std::set<std::string, std::less<>> names{std::string(kBoundsDrawing)};
    for (const std::shared_ptr<DrawerEntry>& entry : drawers) {
        names.insert(entry->name);
    }
    return {names.begin(), names.end()};
}

void DebugPlugin::DrawerEntry::disconnect() {
    if (plugin != nullptr) {
        std::erase_if(plugin->drawers, [this](const std::shared_ptr<DrawerEntry>& entry) { return entry.get() == this; });
        plugin = nullptr;
    }
}

core::Connection DebugPlugin::addDrawer(std::string name, Drawer drawer) {
    auto entry = std::make_shared<DrawerEntry>();
    entry->plugin = this;
    entry->name = std::move(name);
    entry->drawer = std::move(drawer);
    drawers.push_back(entry);
    return core::Connection(std::weak_ptr<core::Connection::Link>(entry));
}

// A drawer may add or remove drawers while it draws, so the loop walks a copy and skips the ones removed meanwhile, and the bounds come last, around everything the canvas holds.
void DebugPlugin::drawOverlay(core::Engine& engine, graphics2d::Renderer& renderer) {
    if (drawings.empty()) {
        return;
    }
    const std::vector<std::shared_ptr<DrawerEntry>> snapshot = drawers;
    for (const std::shared_ptr<DrawerEntry>& entry : snapshot) {
        if (entry->isConnected() && !entry->blocked && drawings.contains(entry->name)) {
            entry->drawer(renderer);
        }
    }
    if (drawings.contains(kBoundsDrawing)) {
        renderer.drawBounds(*engine.getDefaultFont(), kBoundsColor, kBoundsLabelSize);
    }
}

// Every drawing turns off while any is on, and otherwise every drawing the engine and the app know turns on.
void DebugPlugin::toggleDrawings() {
    if (!drawings.empty()) {
        drawings.clear();
        return;
    }
    for (std::string& name : getDrawingNames()) {
        drawings.insert(std::move(name));
    }
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

void DebugPlugin::MonitorEntry::disconnect() {
    if (plugin != nullptr) {
        plugin->removeMonitors([this](const MonitorEntry& entry) { return &entry == this; });
    }
}

core::Connection DebugPlugin::addMonitor(std::string name, debug::Monitor::Sampler sampler) {
    auto entry = std::make_shared<MonitorEntry>();
    entry->monitor = std::make_shared<debug::Monitor>(std::move(name), std::move(sampler));
    removeMonitor(entry->monitor->getName());
    entry->plugin = this;
    monitors.push_back(entry);
    return core::Connection(std::weak_ptr<core::Connection::Link>(entry));
}

bool DebugPlugin::removeMonitor(std::string_view name) {
    return removeMonitors([name](const MonitorEntry& entry) { return entry.monitor->getName() == name; });
}

bool DebugPlugin::removeMonitors(const std::function<bool(const MonitorEntry&)>& matches) {
    // clang-format off
    return std::erase_if(monitors, [&matches](const std::shared_ptr<MonitorEntry>& entry) {
        if (!matches(*entry)) {
            return false;
        }
        entry->plugin = nullptr;
        return true;
    }) > 0;
    // clang-format on
}

std::vector<std::shared_ptr<debug::Monitor>> DebugPlugin::getMonitors() const {
    std::vector<std::shared_ptr<debug::Monitor>> registered;
    registered.reserve(monitors.size());
    for (const std::shared_ptr<MonitorEntry>& entry : monitors) {
        registered.push_back(entry->monitor);
    }
    return registered;
}

void DebugPlugin::event(core::Engine&, const platform::Event& event) {
    if (event.type != platform::Event::Type::KeyDown || event.repeat) {
        return;
    }
    // The key of the statistics wins when the app gives both the same key.
    if (!toggleKey || event.key != *toggleKey) {
        if (drawKey && event.key == *drawKey) {
            toggleDrawings();
        }
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

void DebugPlugin::renderUi(core::Engine& engine, const core::SceneView& view) {
    if (!view.current || statsMode != debug::StatsDisplay::Mode::Full) {
        return;
    }
    ui::Backend& backend = engine.getPlugin<UiPlugin>().getBackend();
    backend.makeCurrent();

    const math::Rect safe = backend.getSafeRect();
    ImGui::SetNextWindowPos({safe.x + 16.0F, safe.y + 16.0F}, ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize({720.0F, std::min(860.0F, safe.height - 32.0F)}, ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowBgAlpha(0.9F);
    if (!debug::OverlayWindow::draw(engine, captureStats(engine), getMonitors(), getRecentLog())) {
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

// The renderer has just submitted the frame, so its statistics are complete, and monitors sample what the frame left behind. A monitor may add or remove monitors while it samples, so the loop walks a copy and skips the ones removed meanwhile.
void DebugPlugin::endFrame(core::Engine& engine) {
    lastRendering = engine.getRenderer2D().getStats();
    const std::vector<std::shared_ptr<MonitorEntry>> snapshot = monitors;
    for (const std::shared_ptr<MonitorEntry>& entry : snapshot) {
        if (entry->isConnected() && !entry->blocked) {
            entry->monitor->sample();
        }
    }
}

void DebugPlugin::installLua(core::Engine&, lua_State* L) {
    debug::DebugLua::install(L);
}

} // namespace haylen::plugins
