#include "haylen/plugins/DebugPlugin.hpp"

#include <imgui.h>

#include <algorithm>
#include <chrono>
#include <format>
#include <stdexcept>
#include <utility>

#include "debug/ConsoleWindow.hpp"
#include "debug/DebugLua.hpp"
#include "debug/Inspector.hpp"
#include "debug/InspectorWindow.hpp"
#include "debug/OverlayWindow.hpp"
#include "haylen/core/Engine.hpp"
#include "haylen/core/EventBus.hpp"
#include "haylen/core/FrameClock.hpp"
#include "haylen/core/JobSystem.hpp"
#include "haylen/core/LifecycleEvent.hpp"
#include "haylen/core/SceneView.hpp"
#include "haylen/debug/ObjectCounter.hpp"
#include "haylen/debug/Profiler.hpp"
#include "haylen/graphics/Device.hpp"
#include "haylen/lua/Runtime.hpp"
#include "haylen/platform/Event.hpp"
#include "haylen/platform/System.hpp"
#include "haylen/plugins/UiPlugin.hpp"
#include "haylen/storage/UserStorage.hpp"
#include "platform/ConsoleRelay.hpp"
#include "platform/CrashGuard.hpp"

namespace haylen::plugins {

DebugPlugin::DebugPlugin(bool developmentRun) : consoleWindow(std::make_unique<debug::ConsoleWindow>()), inspectorWindow(std::make_unique<debug::InspectorWindow>()), development(developmentRun) {}

DebugPlugin::~DebugPlugin() = default;

void DebugPlugin::start(core::Engine& engine) {
    owner = &engine;
    consoleEnabled = development || engine.getConfig().debug.console;
    addBuiltInCommands(engine);
    readLastCrash(engine);
    engine.getSystem().guardCrashes(engine.getStorage().getRoot() / kCrashReportFile);
    breadcrumbsChanged = true;
    listener = core::Log::addListener([this](core::Log::Level level, std::string_view line) { record(level, line); });
    statsMode = engine.getConfig().debug.stats;
    statsDisplay.setItems(engine.getConfig().debug.statsItems);
    setObjectEvents(engine.getConfig().debug.objectEvents);
    drawings.insert(engine.getConfig().debug.drawings.begin(), engine.getConfig().debug.drawings.end());
    overlay = engine.getRenderer2D().addCanvasOverlay([this, &engine](graphics2d::Renderer& renderer) { drawOverlay(engine, renderer); });
}

// The commands of Lua hold functions of the Lua state, which closes after the plugins stop.
void DebugPlugin::stop(core::Engine& engine) {
    for (core::Connection& command : std::exchange(builtInCommands, {})) {
        command.disconnect();
    }
    std::vector<std::string> names;
    for (const debug::Console::Command* command : console.getCommands()) {
        names.push_back(command->name);
    }
    for (const std::string& name : names) {
        if (name != "help") {
            console.removeCommand(name);
        }
    }
    consoleVisible = false;
    inspectorVisible = false;
    detachTrace();
    trace.reset();
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

// A drawer may add or remove drawers while it draws, so the loop walks a copy and skips the ones removed meanwhile, and the bounds come last, around everything the canvas holds. The copy goes back into a member, which keeps its capacity for the next canvas.
void DebugPlugin::drawOverlay(core::Engine& engine, graphics2d::Renderer& renderer) {
    if (drawings.empty()) {
        return;
    }
    std::vector<std::shared_ptr<DrawerEntry>> snapshot = std::exchange(drawerSnapshot, {});
    snapshot.assign(drawers.begin(), drawers.end());
    for (const std::shared_ptr<DrawerEntry>& entry : snapshot) {
        if (entry->isConnected() && !entry->blocked && drawings.contains(entry->name)) {
            entry->drawer(renderer);
        }
    }
    snapshot.clear();
    drawerSnapshot = std::move(snapshot);

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
    logChanged = true;
}

void DebugPlugin::addBreadcrumb(std::string message, core::Json data) {
    const double time = std::chrono::duration<double>(std::chrono::system_clock::now().time_since_epoch()).count();
    owner->getEvents().post(std::string(core::LifecycleEvent::kBreadcrumbAdded), {{"message", message}, {"data", data}, {"time", time}});
    breadcrumbs.push({.time = time, .message = std::move(message), .data = std::move(data)});
    breadcrumbsChanged = true;
}

// A report that a crash left reads once, so the next start without a crash finds none.
void DebugPlugin::readLastCrash(core::Engine& engine) {
    lastCrash = nullptr;
    storage::UserStorage& files = engine.getStorage();
    if (!files.exists(kCrashReportFile)) {
        return;
    }
    core::Json report = core::Json::parse(files.readText(kCrashReportFile), nullptr, false);
    files.remove(kCrashReportFile);
    if (report.is_object() && report.contains("reason") && report.at("reason").is_string()) {
        lastCrash = std::move(report);
    }
}

// The context only changes with a breadcrumb or a log line, so a frame without them builds nothing.
void DebugPlugin::publishCrashContext(core::Engine& engine) {
    core::Json recent = core::Json::array();
    const std::size_t first = breadcrumbs.size() > kReportBreadcrumbs ? breadcrumbs.size() - kReportBreadcrumbs : 0;
    for (std::size_t index = first; index < breadcrumbs.size(); ++index) {
        recent.push_back({{"time", breadcrumbs[index].time}, {"message", cut(breadcrumbs[index].message)}, {"data", breadcrumbs[index].data}});
    }
    core::Json log = core::Json::array();
    {
        const std::scoped_lock lock(linesMutex);
        const std::size_t start = lines.size() > kReportLogLines ? lines.size() - kReportLogLines : 0;
        for (std::size_t index = start; index < lines.size(); ++index) {
            log.push_back({{"level", std::string(kLevelNames[static_cast<std::size_t>(lines[index].level)])}, {"text", cut(lines[index].text)}});
        }
    }
    const core::AppConfig& config = engine.getConfig();
    core::Json context = {{"app", {{"name", config.name}, {"identifier", config.identifier}, {"version", config.version}}}, {"breadcrumbs", std::move(recent)}, {"log", std::move(log)}};
    std::string text = context.dump(-1, ' ', false, core::Json::error_handler_t::replace);
    if (text.size() > platform::CrashGuard::kContextSize) {
        context.erase("log");
        for (core::Json& breadcrumb : context.at("breadcrumbs")) {
            breadcrumb.erase("data");
        }
        text = context.dump(-1, ' ', false, core::Json::error_handler_t::replace);
    }
    engine.getSystem().setCrashContext(text);
}

std::string DebugPlugin::cut(std::string_view text) {
    return std::string(text.substr(0, kReportTextSize));
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
    const bool consoleKeyDown = event.type == platform::Event::Type::KeyDown && !event.repeat && consoleKey && event.key == *consoleKey;
    const bool consoleTouch = event.type == platform::Event::Type::TouchBegan && event.touchCount == kConsoleTouches;
    if (consoleEnabled && (consoleKeyDown || consoleTouch)) {
        setConsoleVisible(!consoleVisible);
        return;
    }
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
    if (!view.current || (statsMode != debug::StatsDisplay::Mode::Full && !consoleVisible && !inspectorVisible)) {
        return;
    }
    ui::Backend& backend = engine.getPlugin<UiPlugin>().getBackend();
    backend.makeCurrent();
    const math::Rect safe = backend.getSafeRect();

    if (statsMode == debug::StatsDisplay::Mode::Full) {
        ImGui::SetNextWindowPos({safe.x + 16.0F, safe.y + 16.0F}, ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize({720.0F, std::min(860.0F, safe.height - 32.0F)}, ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowBgAlpha(0.9F);
        if (!debug::OverlayWindow::draw(engine, captureStats(engine), getMonitors(), getRecentLog())) {
            statsMode = debug::StatsDisplay::Mode::Off;
        }
    }

    if (consoleVisible) {
        ImGui::SetNextWindowPos({safe.x + 16.0F, safe.y + safe.height * 0.4F}, ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize({std::min(900.0F, safe.width - 32.0F), safe.height * 0.6F - 16.0F}, ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowBgAlpha(0.92F);
        consoleVisible = consoleWindow->draw(console);
    }

    if (inspectorVisible) {
        ImGui::SetNextWindowPos({safe.x + safe.width - std::min(560.0F, safe.width - 32.0F) - 16.0F, safe.y + 16.0F}, ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize({std::min(560.0F, safe.width - 32.0F), safe.height * 0.6F}, ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowBgAlpha(0.92F);
        const double now = std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
        inspectorVisible = inspectorWindow->draw(engine.getLuaState(), now);
    }
}

void DebugPlugin::setInspectorVisible(bool value) {
    if (value && !consoleEnabled) {
        throw std::logic_error("The inspector is off outside development runs. Set \"debug.console\" to true in \"app.json\" to turn it on.");
    }
    inspectorVisible = value;
}

void DebugPlugin::setConsoleVisible(bool value) {
    if (value && !consoleEnabled) {
        throw std::logic_error("The console is off outside development runs. Set \"debug.console\" to true in \"app.json\" to turn it on.");
    }
    if (value && !consoleVisible) {
        consoleWindow->focusLine();
    }
    consoleVisible = value;
}

// A trace of a number of frames stops at the start of the next frame, since the profiler records the end of the last one after the plugins end their frame.
void DebugPlugin::beginFrame(core::Engine&, float) {
    if (tracing && traceFramesLeft && *traceFramesLeft <= 0) {
        detachTrace();
    }
    runRelayedLines();
}

void DebugPlugin::runRelayedLines() {
    for (const platform::ConsoleRelay::Request& request : platform::ConsoleRelay::take()) {
        if (!consoleEnabled) {
            request.reply({.ok = false, .output = "The console is off outside development runs. Set \"debug.console\" to true in \"app.json\" to turn it on."});
            continue;
        }
        request.reply(console.run(request.line));
    }
}

void DebugPlugin::startTrace(std::optional<int> frames, std::size_t capacity) {
    if (tracing) {
        throw std::logic_error("A trace is already recording. Stop it with \"debug.stopTraceAsync\" before starting another.");
    }
    if (frames && *frames < 1) {
        throw std::invalid_argument("A trace records at least 1 frame.");
    }
    trace = std::make_shared<debug::TraceRecorder>(capacity);
    trace->nameThread("frame");
    owner->getProfiler().setTraceRecorder(trace);
    owner->getJobs().setTraceRecorder(trace);
    traceFramesLeft = frames;
    lastGpuSeconds = 0.0;
    tracing = true;
}

std::shared_ptr<debug::TraceRecorder> DebugPlugin::stopTrace() {
    detachTrace();
    return std::exchange(trace, nullptr);
}

void DebugPlugin::detachTrace() {
    if (!tracing) {
        return;
    }
    owner->getProfiler().setTraceRecorder(nullptr);
    owner->getJobs().setTraceRecorder(nullptr);
    tracing = false;
}

void DebugPlugin::addBuiltInCommands(core::Engine& engine) {
    using Kind = debug::Console::Kind;
    // clang-format off
    builtInCommands.push_back(console.addCommand({.name = "clear", .help = "Clears the output of the console.", .parameters = {}, .handler = [this](const core::Json&) {
        console.clearOutput();
        return std::string();
    }}));
    builtInCommands.push_back(console.addCommand({.name = "stats", .help = "Shows the statistics as \"off\", \"compact\" or \"full\".", .parameters = {{.kind = Kind::String}}, .handler = [this](const core::Json& arguments) {
        const std::string& name = arguments[0].get_ref<const std::string&>();
        const std::optional<debug::StatsDisplay::Mode> mode = debug::StatsDisplay::modeFromName(name);
        if (!mode) {
            throw std::invalid_argument(std::format("The statistics mode \"{}\" is unknown. It is \"off\", \"compact\" or \"full\".", name));
        }
        statsMode = *mode;
        return std::format("The statistics are \"{}\".", name);
    }}));
    builtInCommands.push_back(console.addCommand({.name = "draw", .help = "Turns a debug drawing on or off, or the other way when the line leaves the state out.", .parameters = {{.kind = Kind::String}, {.kind = Kind::Boolean, .optional = true}}, .handler = [this](const core::Json& arguments) {
        const std::string& name = arguments[0].get_ref<const std::string&>();
        const bool enabled = arguments.size() > 1 ? arguments[1].get<bool>() : !isDrawing(name);
        setDrawing(name, enabled);
        return std::format("The drawing \"{}\" is {}.", name, enabled ? "on" : "off");
    }}));
    builtInCommands.push_back(console.addCommand({.name = "timeScale", .help = "Shows the time scale, or sets it.", .parameters = {{.kind = Kind::Number, .optional = true}}, .handler = [&engine](const core::Json& arguments) {
        if (!arguments.empty()) {
            engine.getClock().setTimeScale(arguments[0].get<double>());
        }
        return std::format("The time scale is {:g}.", engine.getClock().getTimeScale());
    }}));
    builtInCommands.push_back(console.addCommand({.name = "pause", .help = "Pauses the app or resumes it, or switches when the line leaves the state out.", .parameters = {{.kind = Kind::Boolean, .optional = true}}, .handler = [&engine](const core::Json& arguments) {
        engine.setPaused(arguments.empty() ? !engine.isPaused() : arguments[0].get<bool>());
        return std::string(engine.isPaused() ? "The app is paused." : "The app runs.");
    }}));
    builtInCommands.push_back(console.addCommand({.name = "restart", .help = "Restarts the app.", .parameters = {}, .handler = [&engine](const core::Json&) {
        engine.requestRestart();
        return std::string("The app restarts.");
    }}));
    builtInCommands.push_back(console.addCommand({.name = "inspect", .help = "Lists the values at a path of the inspector, such as \"watched.player\", or its roots.", .parameters = {{.kind = Kind::String, .optional = true}}, .handler = [&engine](const core::Json& arguments) {
        const std::vector<std::string> path = debug::Inspector::splitPath(arguments.empty() ? std::string_view() : arguments[0].get_ref<const std::string&>());
        std::string text;
        for (const debug::Inspector::Entry& entry : debug::Inspector::list(engine.getLuaState(), path)) {
            text += std::format("{}{}  {}{}{}", text.empty() ? "" : "\n", entry.key, entry.type, entry.value.empty() ? "" : "  ", entry.value);
        }
        return text;
    }}));
    builtInCommands.push_back(console.addCommand({.name = "set", .help = "Sets the number, boolean or string at a path of the inspector, such as \"set watched.player.speed 300\".", .parameters = {{.kind = Kind::String}, {.kind = Kind::Text}}, .handler = [&engine](const core::Json& arguments) {
        const std::string& path = arguments[0].get_ref<const std::string&>();
        debug::Inspector::set(engine.getLuaState(), debug::Inspector::splitPath(path), arguments[1].get_ref<const std::string&>());
        return std::format("Set \"{}\".", path);
    }}));
    builtInCommands.push_back(console.addCommand({.name = "inspector", .help = "Shows or hides the inspector, or switches it when the line leaves the state out.", .parameters = {{.kind = Kind::Boolean, .optional = true}}, .handler = [this](const core::Json& arguments) {
        setInspectorVisible(arguments.empty() ? !inspectorVisible : arguments[0].get<bool>());
        return std::string(inspectorVisible ? "The inspector shows." : "The inspector is hidden.");
    }}));
    if (development) {
        builtInCommands.push_back(console.addCommand({.name = "lua", .help = "Runs Lua code and prints what it returns, in development runs only.", .parameters = {{.kind = Kind::Text}}, .handler = [&engine](const core::Json& arguments) {
            return runLua(engine, arguments[0].get_ref<const std::string&>());
        }}));
    }
    // clang-format on
}

// The code runs as an expression first, so `player.speed` prints its value, and as statements when it is not one.
std::string DebugPlugin::runLua(core::Engine& engine, const std::string& chunk) {
    lua_State* L = engine.getLuaState();
    const int top = lua_gettop(L);
    const std::string expression = "return " + chunk;
    if (luaL_loadbufferx(L, expression.data(), expression.size(), "=console", "t") != LUA_OK) {
        lua_pop(L, 1);
        if (luaL_loadbufferx(L, chunk.data(), chunk.size(), "=console", "t") != LUA_OK) {
            std::string message = lua_tostring(L, -1);
            lua_settop(L, top);
            throw std::invalid_argument(message);
        }
    }
    lua::Runtime::protectedCall(L, 0, LUA_MULTRET);

    std::string text;
    for (int index = top + 1; index <= lua_gettop(L); ++index) {
        if (index > top + 1) {
            text += "  ";
        }
        switch (lua_type(L, index)) {
        case LUA_TNIL:
            text += "nil";
            break;
        case LUA_TBOOLEAN:
            text += lua_toboolean(L, index) != 0 ? "true" : "false";
            break;
        case LUA_TNUMBER:
        case LUA_TSTRING:
            text += lua_tostring(L, index);
            break;
        default:
            text += std::format("{}: {}", luaL_typename(L, index), lua_topointer(L, index));
            break;
        }
    }
    lua_settop(L, top);
    return text;
}

void DebugPlugin::renderOverlay(core::Engine& engine) {
    if (statsMode != debug::StatsDisplay::Mode::Compact) {
        return;
    }
    statsDisplay.drawCompact(engine, lastRendering);
}

// The renderer has just submitted the frame, so its statistics are complete, and monitors sample what the frame left behind. A monitor may add or remove monitors while it samples, so the loop walks a copy, kept in a member like the copy of the drawers, and skips the ones removed meanwhile.
void DebugPlugin::endFrame(core::Engine& engine) {
    lastRendering = engine.getRenderer2D().getStats();
    if (logChanged.exchange(false) || std::exchange(breadcrumbsChanged, false)) {
        publishCrashContext(engine);
    }
    if (tracing) {
        // The GPU reports a frame once it finished it, so a time that did not change belongs to a frame the trace already holds.
        if (const std::optional<double> gpu = engine.getGraphics().getGpuFrameSeconds(); gpu && *gpu != lastGpuSeconds) {
            trace->recordGpu("gpuFrame", debug::TraceRecorder::now(), *gpu);
            lastGpuSeconds = *gpu;
        }
        if (traceFramesLeft) {
            --*traceFramesLeft;
        }
    }
    std::vector<std::shared_ptr<MonitorEntry>> snapshot = std::exchange(monitorSnapshot, {});
    snapshot.assign(monitors.begin(), monitors.end());
    for (const std::shared_ptr<MonitorEntry>& entry : snapshot) {
        if (entry->isConnected() && !entry->blocked) {
            entry->monitor->sample();
        }
    }
    snapshot.clear();
    monitorSnapshot = std::move(snapshot);
}

void DebugPlugin::installLua(core::Engine&, lua_State* L) {
    debug::DebugLua::install(L);
}

} // namespace haylen::plugins
