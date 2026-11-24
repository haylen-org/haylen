#include "haylen/plugins/DebugPlugin.hpp"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <format>
#include <stdexcept>
#include <utility>

#include "debug/InspectorWindow.hpp"
#include "debug/OverlayWindow.hpp"
#include "haylen/core/Engine.hpp"
#include "haylen/core/EventBus.hpp"
#include "haylen/core/FrameClock.hpp"
#include "haylen/core/JobSystem.hpp"
#include "haylen/core/LifecycleEvent.hpp"
#include "haylen/core/SceneView.hpp"
#include "haylen/debug/Inspector.hpp"
#include "haylen/debug/ObjectCounter.hpp"
#include "haylen/debug/Profiler.hpp"
#include "haylen/graphics/Device.hpp"
#include "haylen/platform/DevelopmentSession.hpp"
#include "haylen/platform/Event.hpp"
#include "haylen/platform/System.hpp"
#include "haylen/plugins/UiPlugin.hpp"
#include "haylen/storage/UserStorage.hpp"
#include "platform/ConsoleRelay.hpp"
#include "platform/CrashGuard.hpp"

namespace haylen::plugins {

DebugPlugin::DebugPlugin(platform::DevelopmentSession* developmentSession) : session(developmentSession), inspectorWindow(std::make_unique<debug::InspectorWindow>()), overlayWindow(std::make_unique<debug::OverlayWindow>()), development(developmentSession != nullptr) {}

DebugPlugin::~DebugPlugin() = default;

void DebugPlugin::start(core::Engine& engine) {
    owner = &engine;
    consoleEnabled = development || engine.getConfig().debug.console;
    addBuiltInCommands(engine);

    // A crash report lives in the user data, so an app whose platform keeps the user data from it neither reads nor writes one.
    if (engine.getStorage().isAvailable()) {
        readLastCrash(engine);
        engine.getSystem().guardCrashes(engine.getStorage().getRoot() / kCrashReportFile);
    }

    breadcrumbsChanged = true;
    listener = core::Log::addListener([this](core::Log::Level level, std::string_view line) { record(level, line); });
    statsMode = engine.getConfig().debug.stats;
    statsDisplay.setItems(engine.getConfig().debug.statsItems);
    setObjectEvents(engine.getConfig().debug.objectEvents);
    drawings.insert(engine.getConfig().debug.drawings.begin(), engine.getConfig().debug.drawings.end());
    overlay = engine.getRenderer2D().addCanvasOverlay([this, &engine](graphics2d::Renderer& renderer) { drawOverlay(engine, renderer); });
}

// The commands that scripts add hold functions of their runtime, which closes after the plugins stop.
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

// Every name keeps the place it had before, so the storage of a vector that the reader keeps holds the same names every frame.
void DebugPlugin::readDrawingNames(std::vector<std::string>& names) const {
    std::size_t count = 0;
    // clang-format off
    const auto add = [&names, &count](std::string_view name) {
        const auto end = names.begin() + static_cast<std::ptrdiff_t>(count);
        if (std::find(names.begin(), end, name) != end) {
            return;
        }

        if (count == names.size()) {
            names.emplace_back();
        }

        names[count++] = name;
    };
    // clang-format on
    add(kBoundsDrawing);
    for (const std::shared_ptr<DrawerEntry>& entry : drawers) {
        add(entry->name);
    }

    names.resize(count);
    std::ranges::sort(names);
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

// A drawer may add or remove drawers while it draws, so the loop walks a copy and skips the ones removed meanwhile, and the bounds come last, around everything the canvas holds. The copy goes back into a member, which keeps its capacity for the next canvas. A canvas that turns its debug drawings off, such as the one of the UI, gets none.
void DebugPlugin::drawOverlay(core::Engine& engine, graphics2d::Renderer& renderer) {
    if (drawings.empty() || !renderer.getCanvasOptions().debugDrawings) {
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

void DebugPlugin::toggleDrawings() {
    if (!drawings.empty()) {
        drawings.clear();
        return;
    }

    std::vector<std::string> names;
    readDrawingNames(names);
    for (std::string& name : names) {
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
    core::Json context = {{"app", {{"name", config.name}, {"identifier", config.identifier}, {"version", config.version}, {"build", config.build}}}, {"breadcrumbs", std::move(recent)}, {"log", std::move(log)}};
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

void DebugPlugin::readRecentLog(std::vector<debug::LogLine>& recent) const {
    const std::scoped_lock lock(linesMutex);
    recent.resize(lines.size());
    for (std::size_t index = 0; index < lines.size(); ++index) {
        recent[index].level = lines[index].level;
        recent[index].text = lines[index].text;
    }
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

void DebugPlugin::updateStats(core::Engine& engine, debug::Stats& stats) const {
    stats.update(engine, lastRendering);
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

void DebugPlugin::readMonitors(std::vector<std::shared_ptr<debug::Monitor>>& registered) const {
    registered.resize(monitors.size());
    for (std::size_t index = 0; index < monitors.size(); ++index) {
        registered[index] = monitors[index]->monitor;
    }
}

void DebugPlugin::event(core::Engine&, const platform::Event& event) {
    if (event.type != platform::Event::Type::KeyDown || event.repeat) {
        return;
    }

    // The key of the statistics wins when the app gives two tools the same key, and the key of the inspector comes next.
    if (toggleKey && event.key == *toggleKey) {
        cycleStatsMode();
        return;
    }

    if (consoleEnabled && inspectorKey && event.key == *inspectorKey) {
        setInspectorVisible(!inspectorVisible);
        return;
    }

    if (drawKey && event.key == *drawKey) {
        toggleDrawings();
    }
}

void DebugPlugin::cycleStatsMode() noexcept {
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
    if (!view.current || (statsMode != debug::StatsDisplay::Mode::Full && !inspectorVisible)) {
        return;
    }

    ui::Backend& backend = engine.getPlugin<UiPlugin>().getBackend();
    backend.makeCurrent();

    if (statsMode == debug::StatsDisplay::Mode::Full && !overlayWindow->draw(engine, backend, *this)) {
        statsMode = debug::StatsDisplay::Mode::Off;
    }

    if (inspectorVisible) {
        inspectorVisible = inspected != nullptr && inspectorWindow->draw(*inspected, backend);
    }
}

void DebugPlugin::setInspectorClock(std::function<double()> clock) {
    inspectorWindow = std::make_unique<debug::InspectorWindow>(std::move(clock));
}

std::string_view DebugPlugin::getInspectorLine() const noexcept {
    return inspectorWindow->getLine();
}

void DebugPlugin::setInspectorVisible(bool value) {
    if (value && !consoleEnabled) {
        throw std::logic_error("The inspector is off outside development runs. Set \"debug.console\" to true in \"haylen.json\" to turn it on.");
    }

    inspectorVisible = value;
}

// A trace of a number of frames stops at the start of the next frame, since the profiler records the end of the last one after the plugins end their frame.
void DebugPlugin::beginFrame(core::Engine& engine, float) {
    if (tracing && traceFramesLeft && *traceFramesLeft <= 0) {
        detachTrace();
    }

    runRelayedLines(engine);
}

bool DebugPlugin::isAnimating() const {
    return statsMode != debug::StatsDisplay::Mode::Off || inspectorVisible || tracing;
}

// The lines of the console and the inspections may change what the app shows, which an app that redraws on change draws in the next frame.
void DebugPlugin::runRelayedLines(core::Engine& engine) {
    std::vector<platform::ConsoleRelay::Request> requests = platform::ConsoleRelay::take();
    if (!requests.empty()) {
        engine.requestRedraw();
    }

    for (const platform::ConsoleRelay::Request& request : requests) {
        if (!consoleEnabled) {
            request.reply({.ok = false, .output = "The console is off outside development runs. Set \"debug.console\" to true in \"haylen.json\" to turn it on."});
            continue;
        }

        request.reply(console.run(request.line));
    }

    for (const platform::ConsoleRelay::ListReply& reply : platform::ConsoleRelay::takeCommandRequests()) {
        reply(consoleEnabled ? describeCommands() : core::Json::array());
    }

    for (const platform::ConsoleRelay::Inspection& inspection : platform::ConsoleRelay::takeInspections()) {
        answerInspection(inspection.path, inspection.reply);
    }
}

// Inspecting reads the values of the app and never changes them, so a tool may look at the app while it runs.
void DebugPlugin::answerInspection(const std::vector<std::string>& path, const std::function<void(bool ok, const core::Json& result)>& reply) const {
    if (!consoleEnabled) {
        reply(false, {{"code", "unsupported"}, {"message", "The inspector is off outside development runs. Set \"debug.console\" to true in \"haylen.json\" to turn it on."}});
        return;
    }

    core::Json entries = core::Json::array();
    try {
        debug::Inspector::Listing listing;
        getInspected().list(path, listing);
        for (const debug::Inspector::Entry& entry : listing.entries) {
            entries.push_back({{"key", entry.key}, {"type", entry.type}, {"value", entry.value}, {"expandable", entry.expandable}});
        }
    } catch (const std::exception& error) {
        reply(false, {{"code", "invalidPath"}, {"message", error.what()}});
        return;
    }
    reply(true, {{"path", debug::Inspector::joinPath(path)}, {"entries", std::move(entries)}});
}

core::Json DebugPlugin::describeCommands() const {
    core::Json commands = core::Json::array();
    for (const debug::Console::Command* command : console.getCommands()) {
        core::Json arguments = core::Json::array();
        for (const debug::Console::Parameter& parameter : command->parameters) {
            arguments.push_back(std::string(debug::Console::kindName(parameter.kind)) + (parameter.optional ? "?" : ""));
        }

        commands.push_back({{"name", command->name}, {"usage", debug::Console::describeUsage(*command)}, {"help", command->help}, {"arguments", std::move(arguments)}});
    }

    return commands;
}

void DebugPlugin::captureGpu(int frames, graphics::Device::CaptureCallback done) {
    graphics::Device& graphics = owner->getGraphics();
    const std::filesystem::path folder = owner->getStorage().getRoot() / kGpuCaptureFolder;
    if (graphics.getCaptureSupport().tool != gpu::Device::CaptureTool::None) {
        std::filesystem::create_directories(folder);
    }

    // clang-format off
    graphics.captureFrames(frames, folder / makeCaptureName(), [this, frames, done = std::move(done)](std::optional<std::filesystem::path> path, const std::string& error) {
        reportCapture(frames, path, error);
        done(std::move(path), error);
    });
    // clang-format on
}

// A capture is named after the time it started, in UTC to the millisecond, so the names sort in the order the captures were made.
std::string DebugPlugin::makeCaptureName() {
    const auto now = std::chrono::floor<std::chrono::milliseconds>(std::chrono::system_clock::now());
    const auto day = std::chrono::floor<std::chrono::days>(now);
    const std::chrono::year_month_day date{day};
    const std::int64_t milliseconds = (now - day).count();
    return std::format("{:04}-{:02}-{:02}-{:02}-{:02}-{:02}-{:03}", static_cast<int>(date.year()), static_cast<unsigned>(date.month()), static_cast<unsigned>(date.day()), milliseconds / 3600000, milliseconds / 60000 % 60, milliseconds / 1000 % 60, milliseconds % 1000);
}

void DebugPlugin::reportCapture(int frames, const std::optional<std::filesystem::path>& path, const std::string& error) {
    if (!path) {
        const std::string line = std::format("The GPU capture failed: {}", error);
        core::Log::warning("{}", line);

        if (session != nullptr) {
            session->report({{"gpuCapture", {{"ok", false}, {"error", error}}}});
        }

        return;
    }

    const std::u8string text = path->u8string();
    const std::string shown(text.begin(), text.end());
    const std::string_view tool = gpu::Device::kCaptureToolNames[static_cast<std::size_t>(owner->getGraphics().getCaptureSupport().tool)].first;
    const std::string line = std::format("The GPU capture of {} {} is in \"{}\".", frames, frames == 1 ? "frame" : "frames", shown);
    core::Log::info("{}", line);

    if (session != nullptr) {
        session->report({{"gpuCapture", {{"ok", true}, {"path", shown}, {"tool", tool}, {"frames", frames}}}});
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
    debug::TraceRecorder::setThreadRecorder(trace.get());
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
    debug::TraceRecorder::setThreadRecorder(nullptr);
    tracing = false;
}

void DebugPlugin::addBuiltInCommands(core::Engine& engine) {
    using Kind = debug::Console::Kind;
    // clang-format off
    builtInCommands.push_back(console.addCommand({.name = "stats", .help = "Shows the statistics in the mode \"off\", \"compact\" or \"full\", or in the next mode when the line names none.", .parameters = {{.kind = Kind::String, .optional = true}}, .handler = [this](const core::Json& arguments) {
        if (arguments.empty()) {
            cycleStatsMode();
            return std::format("The statistics are \"{}\".", debug::StatsDisplay::modeName(statsMode));
        }

        const std::string& name = arguments[0].get_ref<const std::string&>();
        const std::optional<debug::StatsDisplay::Mode> mode = debug::StatsDisplay::modeFromName(name);
        if (!mode) {
            throw std::invalid_argument(std::format("The statistics mode \"{}\" is unknown. It is \"off\", \"compact\" or \"full\".", name));
        }

        statsMode = *mode;
        return std::format("The statistics are \"{}\".", name);
    }}));
    builtInCommands.push_back(console.addCommand({.name = "draw", .help = "Turns the debug drawing it names on or off, such as \"physics\" or \"bounds\", or switches it when the line leaves the state out. Without a name it switches every drawing.", .parameters = {{.kind = Kind::String, .optional = true}, {.kind = Kind::Boolean, .optional = true}}, .handler = [this](const core::Json& arguments) {
        if (arguments.empty()) {
            toggleDrawings();
            return std::string(drawings.empty() ? "The drawings are off." : "The drawings are on.");
        }

        const std::string& name = arguments[0].get_ref<const std::string&>();
        std::vector<std::string> names;
        readDrawingNames(names);
        if (std::ranges::find(names, name) == names.end()) {
            throw std::invalid_argument(std::format("There is no drawing \"{}\". The drawings are {}.", name, describeNames(names)));
        }

        const bool enabled = arguments.size() > 1 ? arguments[1].get<bool>() : !isDrawing(name);
        setDrawing(name, enabled);
        return std::format("The drawing \"{}\" is {}.", name, enabled ? "on" : "off");
    }}));
    builtInCommands.push_back(console.addCommand({.name = "timeScale", .help = "Sets the time scale of the app, such as 0.5 for half speed, or shows it when the line leaves it out.", .parameters = {{.kind = Kind::Number, .optional = true}}, .handler = [&engine](const core::Json& arguments) {
        if (!arguments.empty()) {
            engine.getClock().setTimeScale(arguments[0].get<double>());
        }

        return std::format("The time scale is {:g}.", engine.getClock().getTimeScale());
    }}));
    builtInCommands.push_back(console.addCommand({.name = "pause", .help = "Pauses or resumes the app, or switches between the two when the line leaves the state out.", .parameters = {{.kind = Kind::Boolean, .optional = true}}, .handler = [&engine](const core::Json& arguments) {
        engine.setPaused(arguments.empty() ? !engine.isPaused() : arguments[0].get<bool>());
        return std::string(engine.isPaused() ? "The app is paused." : "The app runs.");
    }}));
    builtInCommands.push_back(console.addCommand({.name = "restart", .help = "Restarts the app.", .parameters = {}, .handler = [&engine](const core::Json&) {
        engine.requestRestart();
        return std::string("The app restarts.");
    }}));
    builtInCommands.push_back(console.addCommand({.name = "inspect", .help = "Lists the values at a path of the inspector, such as \"scenes.1\", or its roots when the line names none.", .parameters = {{.kind = Kind::String, .optional = true}}, .handler = [this](const core::Json& arguments) {
        const std::vector<std::string> path = debug::Inspector::splitPath(arguments.empty() ? std::string_view() : arguments[0].get_ref<const std::string&>());
        std::string text;
        debug::Inspector::Listing listing;
        getInspected().list(path, listing);
        for (const debug::Inspector::Entry& entry : listing.entries) {
            text += std::format("{}{}  {}{}{}", text.empty() ? "" : "\n", entry.key, entry.type, entry.value.empty() ? "" : "  ", entry.value);
        }

        return text;
    }}));
    builtInCommands.push_back(console.addCommand({.name = "set", .help = "Sets a number, a boolean or a string at a path of the inspector, such as \"set watched.player.speed 300\".", .parameters = {{.kind = Kind::String}, {.kind = Kind::Text}}, .handler = [this](const core::Json& arguments) {
        const std::string& path = arguments[0].get_ref<const std::string&>();
        getInspected().set(debug::Inspector::splitPath(path), arguments[1].get_ref<const std::string&>());
        return std::format("Set \"{}\".", path);
    }}));
    builtInCommands.push_back(console.addCommand({.name = "inspector", .help = "Shows or hides the inspector window, or switches it when the line leaves the state out.", .parameters = {{.kind = Kind::Boolean, .optional = true}}, .handler = [this](const core::Json& arguments) {
        setInspectorVisible(arguments.empty() ? !inspectorVisible : arguments[0].get<bool>());
        return std::string(inspectorVisible ? "The inspector shows." : "The inspector is hidden.");
    }}));
    builtInCommands.push_back(console.addCommand({.name = "gpuCapture", .help = "Captures the next frames of the GPU, 1 unless the line names more, with the capture tool of the graphics API, and prints the path of the file once it ends.", .parameters = {{.kind = Kind::Integer, .optional = true}}, .handler = [this, &engine](const core::Json& arguments) {
        const int frames = arguments.empty() ? 1 : arguments[0].get<int>();
        captureGpu(frames, [](std::optional<std::filesystem::path>, const std::string&) {});
        const std::string_view tool = gpu::Device::kCaptureToolNames[static_cast<std::size_t>(engine.getGraphics().getCaptureSupport().tool)].first;
        return std::format("Capturing the next {} {} with \"{}\".", frames, frames == 1 ? "frame" : "frames", tool);
    }}));
    // clang-format on
}

// Names a list of names as a sentence does, such as `"bounds" and "physics"`.
std::string DebugPlugin::describeNames(const std::vector<std::string>& names) {
    std::string text;
    for (std::size_t index = 0; index < names.size(); ++index) {
        const std::string_view separator = index == 0 ? "" : (index + 1 == names.size() ? " and " : ", ");
        text += std::format("{}\"{}\"", separator, names[index]);
    }

    return text;
}

void DebugPlugin::setScriptMemory(std::function<std::size_t()> reader) {
    scriptMemory = std::move(reader);
}

std::size_t DebugPlugin::getScriptMemory() const {
    return scriptMemory ? scriptMemory() : 0;
}

void DebugPlugin::setScriptSignals(std::function<void(std::vector<core::EventBus::Topic>&)> reader) {
    scriptSignals = std::move(reader);
}

// The reader fills the storage of the list again, so the signals that the overlay shows every frame allocate nothing.
void DebugPlugin::readScriptSignals(std::vector<core::EventBus::Topic>& signals) const {
    if (scriptSignals) {
        scriptSignals(signals);
        return;
    }

    signals.clear();
}

void DebugPlugin::setInspected(debug::Inspector::Provider* value) noexcept {
    inspected = value;
}

debug::Inspector::Provider& DebugPlugin::getInspected() const {
    if (inspected == nullptr) {
        throw std::logic_error("The app holds no values that the inspector lists, since none of its code runs as scripts.");
    }

    return *inspected;
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

} // namespace haylen::plugins
