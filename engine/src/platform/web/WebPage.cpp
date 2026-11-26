#include "platform/web/WebPage.hpp"

#include <emscripten/emscripten.h>

#include <algorithm>
#include <cstdlib>
#include <exception>
#include <format>
#include <map>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "haylen/2d/graphics/Renderer.hpp"
#include "haylen/assets/Manager.hpp"
#include "haylen/audio/Mixer.hpp"
#include "haylen/content/Bootstrap.hpp"
#include "haylen/core/AppConfig.hpp"
#include "haylen/core/Engine.hpp"
#include "haylen/core/Json.hpp"
#include "haylen/debug/Inspector.hpp"
#include "haylen/debug/Profiler.hpp"
#include "haylen/io/MemoryPackage.hpp"
#include "haylen/io/Package.hpp"
#include "haylen/io/Path.hpp"
#include "haylen/platform/Battery.hpp"
#include "haylen/platform/DevelopmentSession.hpp"
#include "haylen/platform/Event.hpp"
#include "haylen/platform/PluginStreams.hpp"
#include "haylen/platform/Theme.hpp"
#include "haylen/platform/native/HaylenNative.h"
#include "io/OverlayPackage.hpp"
#include "platform/AccessibilityRelay.hpp"
#include "platform/BridgeRelay.hpp"
#include "platform/ConsoleRelay.hpp"
#include "platform/LinkRelay.hpp"
#include "platform/NativeHost.hpp"
#include "platform/Player.hpp"
#include "platform/ScreenRelay.hpp"
#include "platform/Services.hpp"
#include "platform/web/WebEntry.hpp"
#include "platform/web/WebTextInput.hpp"

// clang-format off
EM_JS(void, haylen_js_started, (const char* json), {
    Module.haylen.reportStarted(JSON.parse(UTF8ToString(json)));
});

EM_JS(void, haylen_js_declare_orientation, (int value), {
    Module.haylen.declareOrientation(value);
});

EM_JS(double, haylen_js_project_budget, (int bytes), {
    const budget = Module.haylen.projectBudget;
    return bytes !== 0 ? budget.bytes : budget.files;
});

EM_JS(void, haylen_js_complete_request, (double request, int ok, const char* json), {
    Module.haylen.completeRequest(request, ok !== 0, JSON.parse(UTF8ToString(json)));
});

EM_JS(void, haylen_js_set_drawable_width, (int width), {
    Module.haylen.setDrawableWidth(width);
});

EM_JS(void, haylen_js_end_splash, (float fadeOut), {
    Module.haylen.reportSplashEnded(fadeOut);
});

EM_JS(void, haylen_js_stopped, (), {
    Module.haylen.reportStopped();
});

EM_JS(void, haylen_js_stats, (const char* json), {
    Module.haylen.reportStats(JSON.parse(UTF8ToString(json)));
});

EM_JS(void, haylen_js_reloaded, (const char* json), {
    Module.haylen.reportReloaded(JSON.parse(UTF8ToString(json)));
});

EM_JS(void, haylen_js_set_fullscreen, (double id, int value, const char* display), {
    Module.haylen.setFullscreen(id, value !== 0, UTF8ToString(display));
});

EM_JS(int, haylen_js_fullscreen, (), {
    return Module.haylen.isFullscreen() ? 1 : 0;
});

EM_JS(void, haylen_js_keep_screen_on, (double id, int value), {
    Module.haylen.keepScreenOn(id, value !== 0);
});

EM_JS(void, haylen_js_await_pointer_lock, (double id, int value), {
    Module.haylen.awaitPointerLock(id, value !== 0);
});

EM_JS(void, haylen_js_lock_pointer, (double id, int value), {
    Module.haylen.lockPointer(id, value !== 0);
});

EM_JS(void, haylen_js_write_clipboard, (double id, const char* text), {
    Module.haylen.writeClipboard(id, UTF8ToString(text));
});

EM_JS(void, haylen_js_read_clipboard, (double id), {
    Module.haylen.readClipboard(id);
});

EM_JS(void, haylen_js_request_every_screen, (double id), {
    Module.haylen.requestEveryScreen(id);
});

EM_JS(char*, haylen_js_clipboard, (), {
    return stringToNewUTF8(Module.haylen.clipboardText());
});

EM_JS(char*, haylen_js_canvas_selector, (), {
    return stringToNewUTF8(Module.haylen.canvasSelector());
});

EM_JS(char*, haylen_js_editor_location, (), {
    const storageNamespace = Module.haylen.storageNamespace;
    return stringToNewUTF8(self.location.origin + (storageNamespace === undefined ? "" : "/" + storageNamespace));
});
// clang-format on

namespace haylen::platform {

std::vector<std::string>& WebPage::touched = *new std::vector<std::string>();
int WebPage::revision = 0;
std::vector<WebPage::PendingRequest>& WebPage::pendingApplies = *new std::vector<PendingRequest>();
std::vector<WebPage::PendingRequest>& WebPage::pendingRuns = *new std::vector<PendingRequest>();
std::vector<core::Json>& WebPage::frameReports = *new std::vector<core::Json>();
std::string& WebPage::lastError = *new std::string();
std::unordered_map<std::uint64_t, WebPage::GatedCallback>& WebPage::gatedCalls = *new std::unordered_map<std::uint64_t, GatedCallback>();
std::uint64_t WebPage::nextGatedCall = 1;
double WebPage::lastStats = 0.0;

template <typename Body> int WebPage::answer(Body&& body) {
    try {
        return body();
    } catch (const RequestError& error) {
        lastError = core::Json{{"code", error.getCode()}, {"message", error.what()}}.dump();
        return -1;
    } catch (const std::exception& error) {
        lastError = core::Json{{"code", "failed"}, {"message", error.what()}}.dump();
        return -1;
    }
}

bool WebPage::isFullscreen() noexcept {
    return haylen_js_fullscreen() != 0;
}

void WebPage::setDrawableWidth(int width) {
    haylen_js_set_drawable_width(width);
}

void WebPage::setFullscreen(bool value, std::string_view display, std::function<void(SystemAnswer answer)> callback) {
    haylen_js_set_fullscreen(awaitGatedCall(withoutText(std::move(callback))), value ? 1 : 0, std::string(display).c_str());
}

void WebPage::setScreenKeptOn(bool value, std::function<void(SystemAnswer answer)> callback) {
    haylen_js_keep_screen_on(awaitGatedCall(withoutText(std::move(callback))), value ? 1 : 0);
}

void WebPage::awaitPointerLock(bool value, std::function<void(SystemAnswer answer)> callback) {
    haylen_js_await_pointer_lock(awaitGatedCall(withoutText(std::move(callback))), value ? 1 : 0);
}

void WebPage::lockPointer(bool value, std::function<void(SystemAnswer answer)> callback) {
    haylen_js_lock_pointer(awaitGatedCall(withoutText(std::move(callback))), value ? 1 : 0);
}

std::optional<std::string> WebPage::refuseMouseMode(Window::MouseMode value) {
    if (value != Window::MouseMode::Confined && value != Window::MouseMode::ConfinedHidden) {
        return std::nullopt;
    }

    return std::format("A browser keeps no pointer inside a page, so the mouse cannot take the mode \"{}\".", Window::mouseModeName(value));
}

void WebPage::setClipboard(std::string_view text, std::function<void(SystemAnswer answer)> callback) {
    haylen_js_write_clipboard(awaitGatedCall(withoutText(std::move(callback))), std::string(text).c_str());
}

void WebPage::readClipboard(std::function<void(SystemAnswer answer, std::string text)> callback) {
    haylen_js_read_clipboard(awaitGatedCall(std::move(callback)));
}

void WebPage::requestEveryScreen(std::function<void(SystemAnswer answer)> callback) {
    haylen_js_request_every_screen(awaitGatedCall(withoutText(std::move(callback))));
}

double WebPage::awaitGatedCall(GatedCallback callback) {
    const std::uint64_t id = nextGatedCall++;
    gatedCalls.emplace(id, std::move(callback));
    return static_cast<double>(id);
}

WebPage::GatedCallback WebPage::withoutText(std::function<void(SystemAnswer answer)> callback) {
    return [callback = std::move(callback)](SystemAnswer answer, std::string) { callback(std::move(answer)); };
}

void WebPage::answerGatedCall(double id, const char* json) {
    const auto found = gatedCalls.find(static_cast<std::uint64_t>(id));
    if (found == gatedCalls.end()) {
        return;
    }

    const GatedCallback callback = std::move(found->second);
    gatedCalls.erase(found);
    const core::Json value = core::Json::parse(json, nullptr, false);
    const auto text = value.is_object() ? value.find("text") : value.end();
    callback(SystemAnswer::fromJson(value), text != value.end() && text->is_string() ? text->get<std::string>() : std::string());
}

std::string WebPage::getClipboard() {
    char* text = haylen_js_clipboard();
    std::string result(text);
    std::free(text);
    return result;
}

std::string WebPage::getCanvasSelector() {
    char* selector = haylen_js_canvas_selector();
    std::string result(selector);
    std::free(selector);
    return result;
}

// The page learns the orientation of `haylen.json` with the start of the app, so a handheld page held the other way asks the person to turn the device.
void WebPage::reportStarted(core::Engine& engine) {
    lastStats = emscripten_get_now();
    const core::AppConfig& config = engine.getConfig();
    const core::Json app{{"name", config.name}, {"identifier", config.identifier}, {"version", config.version}, {"build", config.build}};
    haylen_js_declare_orientation(static_cast<int>(config.orientation));
    haylen_js_started(app.dump().c_str());
    const core::Json error = engine.getErrorRecord();
    for (const PendingRequest& pending : std::exchange(pendingRuns, {})) {
        completeRequest(pending.request, true, {{"revision", pending.revision}, {"app", app}, {"error", error}});
    }
}

void WebPage::reportStartFailed(const core::Json& error) {
    for (const PendingRequest& pending : std::exchange(pendingRuns, {})) {
        completeRequest(pending.request, true, {{"revision", pending.revision}, {"app", core::Json()}, {"error", error}});
    }
}

void WebPage::reportStopped() noexcept {
    haylen_js_stopped();
}

void WebPage::reportFrame(core::Engine& engine) {
    const double now = emscripten_get_now();
    if (now - lastStats < kStatsInterval) {
        return;
    }

    lastStats = now;
    haylen_js_stats(getFrameStatistics(engine).dump().c_str());
}

// A frame that took the changes of the requests answers them, with the report of hot reload when the changes did anything. Changes that touched no file the app loaded answer with no report.
void WebPage::endSplashScreen(float fadeOutSeconds) {
    haylen_js_end_splash(fadeOutSeconds);
}

void WebPage::finishFrame() {
    const DevelopmentSession* session = Player::getDevelopmentSession();
    if (pendingApplies.empty() || (session != nullptr && session->hasChanges())) {
        frameReports.clear();
        return;
    }

    const core::Json report = frameReports.empty() ? core::Json() : frameReports.back();
    frameReports.clear();
    for (const PendingRequest& pending : std::exchange(pendingApplies, {})) {
        completeRequest(pending.request, true, {{"revision", pending.revision}, {"report", report}});
    }
}

// A result may hold text of the app that is not UTF-8, such as the output of a console line or a value of the inspector, whose wrong bytes the page receives replaced.
void WebPage::completeRequest(double request, bool ok, const core::Json& result) {
    haylen_js_complete_request(request, ok ? 1 : 0, result.dump(-1, ' ', false, core::Json::error_handler_t::replace).c_str());
}

const char* WebPage::getLastError() noexcept {
    return lastError.c_str();
}

void WebPage::requireEditable() {
    if (content::Bootstrap::find() != nullptr) {
        throw RequestError("notEditable", "The release runtime of the app plays only its protected release, so the page cannot change its files.");
    }
}

void WebPage::requirePlaying() {
    if (!Player::isPlaying()) {
        throw RequestError("notRunning", "No app plays, so there is nothing to apply the changes to. Run the app first.");
    }
}

std::string WebPage::readPath(const core::Json& operation, const char* key) {
    const auto found = operation.find(key);
    if (found == operation.end() || !found->is_string()) {
        throw RequestError("invalidRequest", std::string("An edit needs the path \"") + key + "\" as a string.");
    }

    try {
        return io::Path::normalize(found->get<std::string>());
    } catch (const std::invalid_argument& error) {
        throw RequestError("invalidPath", error.what());
    }
}

// An edit is `{op: "write", path, bytes}` with the index of its buffer, `{op: "remove", path}` or `{op: "rename", from, to}`.
std::vector<WebPage::Edit> WebPage::readEdits(const char* operations, std::span<const std::vector<std::byte>> contents) {
    const core::Json list = core::Json::parse(operations, nullptr, false);
    if (!list.is_array()) {
        throw RequestError("invalidRequest", "The edits of a batch are a list of operations.");
    }

    std::vector<Edit> edits;
    edits.reserve(list.size());
    for (const core::Json& operation : list) {
        const auto op = operation.is_object() ? operation.find("op") : operation.end();
        const std::string name = op != operation.end() && op->is_string() ? op->get<std::string>() : std::string();
        const auto kind = std::ranges::find(kEditNames, name);
        if (kind == kEditNames.end()) {
            throw RequestError("invalidRequest", "An edit is a \"write\", a \"remove\" or a \"rename\", not \"" + name + "\".");
        }

        Edit edit{.kind = static_cast<Edit::Kind>(kind - kEditNames.begin())};
        if (edit.kind == Edit::Kind::Rename) {
            edit.path = readPath(operation, "from");
            edit.target = readPath(operation, "to");
        } else {
            edit.path = readPath(operation, "path");
        }

        if (edit.kind == Edit::Kind::Write) {
            const auto buffer = operation.find("bytes");
            if (buffer == operation.end() || !buffer->is_number_unsigned() || buffer->get<std::size_t>() >= contents.size()) {
                throw RequestError("invalidRequest", "The write of \"" + edit.path + "\" names no buffer of the batch.");
            }

            const std::vector<std::byte>& content = contents[buffer->get<std::size_t>()];
            const auto* first = reinterpret_cast<const std::uint8_t*>(content.data());
            edit.bytes.assign(first, first + content.size());
        }

        edits.push_back(std::move(edit));
    }

    return edits;
}

// The batch plays its edits in order against what the package holds, so a remove or a rename that finds no file fails the whole batch before it changed anything.
void WebPage::checkEdits(const std::vector<Edit>& edits) {
    FileSizes files = measure(Player::getOverlay());
    for (const Edit& edit : edits) {
        if (edit.kind == Edit::Kind::Write) {
            files[edit.path] = edit.bytes.size();
            continue;
        }

        const auto found = files.find(edit.path);
        if (found == files.end()) {
            throw RequestError("missingFile", "The package holds no file \"" + edit.path + "\" to " + (edit.kind == Edit::Kind::Remove ? "remove." : "rename."));
        }

        const std::uint64_t size = found->second;
        files.erase(found);
        if (edit.kind == Edit::Kind::Rename) {
            files[edit.target] = size;
        }
    }

    checkBudget(files);
}

WebPage::FileSizes WebPage::measure(const io::Package& package) {
    FileSizes files;
    for (const std::string& path : package.list("")) {
        files[path] = package.getFileSize(path);
    }

    return files;
}

void WebPage::checkBudget(const FileSizes& files) {
    std::uint64_t bytes = 0;
    for (const auto& [path, size] : files) {
        bytes += size;
    }

    const auto allowedFiles = static_cast<std::uint64_t>(haylen_js_project_budget(0));
    const auto allowedBytes = static_cast<std::uint64_t>(haylen_js_project_budget(1));
    if (files.size() > allowedFiles || bytes > allowedBytes) {
        throw RequestError("projectBudget", "The project may hold " + std::to_string(allowedFiles) + " files and " + std::to_string(allowedBytes) + " bytes, and the package would hold " + std::to_string(files.size()) + " files and " + std::to_string(bytes) + " bytes.");
    }
}

int WebPage::edit(const char* operations, const std::uint32_t* buffers, int count) {
    // clang-format off
    return answer([&] {
        requireEditable();
        const std::vector<std::vector<std::byte>> contents = readBuffers(buffers, count);
        std::vector<Edit> edits = readEdits(operations, contents);
        checkEdits(edits);

        io::OverlayPackage& overlay = Player::getOverlay();
        for (Edit& change : edits) {
            if (change.kind == Edit::Kind::Write) {
                overlay.setFile(change.path, std::move(change.bytes));
            } else if (change.kind == Edit::Kind::Remove) {
                overlay.removeFile(change.path);
            } else {
                overlay.setFile(change.target, overlay.read(change.path));
                overlay.removeFile(change.path);
                touched.push_back(change.target);
            }
            touched.push_back(std::move(change.path));
        }
        return ++revision;
    });
    // clang-format on
}

// The project of an editor comes from the page and its storage namespace, which names a project whose `haylen.json` names no identifier, so the identifier and the database of its user data belong to the same project.
std::string WebPage::editorLocation() {
    char* location = haylen_js_editor_location();
    std::string text(location);
    std::free(location);
    return text;
}

// The app stops, and the empty package waits for the files of the page until `run` starts it.
int WebPage::clearFiles() {
    // clang-format off
    return answer([] {
        requireEditable();
        touched.clear();
        Player::stage(std::make_shared<io::OverlayPackage>(std::make_shared<io::MemoryPackage>("editor", std::map<std::string, std::vector<std::uint8_t>>{}, editorLocation())));
        return ++revision;
    });
    // clang-format on
}

// Without development there is no module graph to reload modules in place, so the app restarts with the edited files, and the request answers at once.
int WebPage::applyChanges(double request) {
    // clang-format off
    return answer([request] {
        requireEditable();
        requirePlaying();
        const std::vector<std::string> changed = std::exchange(touched, {});
        if (DevelopmentSession* session = Player::getDevelopmentSession()) {
            session->addChanges(changed);
            pendingApplies.push_back({.request = request, .revision = revision});
            return 1;
        }

        const std::string reason = "The page outside development restarts the app with its changes.";
        core::Json files = core::Json::array();
        for (const std::string& path : changed) {
            files.push_back({{"path", path}, {"change", "restart"}, {"reason", reason}});
        }

        Player::restart();
        completeRequest(request, true, {{"revision", revision}, {"report", {{"restarted", true}, {"reason", reason}, {"files", std::move(files)}}}});
        return 1;
    });
    // clang-format on
}

int WebPage::runFiles(double request) {
    // clang-format off
    return answer([request] {
        requireEditable();
        touched.clear();
        pendingRuns.push_back({.request = request, .revision = revision});
        Player::restart();
        return 1;
    });
    // clang-format on
}

int WebPage::loadZip(double request, const std::uint8_t* bytes, int size) {
    // clang-format off
    return answer([&] {
        requireEditable();
        std::shared_ptr<io::Package> package;
        try {
            package = io::Package::openZip(std::vector<std::uint8_t>(bytes, bytes + size), "app.zip", editorLocation());
        } catch (const std::exception& error) {
            throw RequestError("package", error.what());
        }
        checkBudget(measure(*package));

        touched.clear();
        pendingRuns.push_back({.request = request, .revision = ++revision});
        Player::restart(std::move(package));
        return 1;
    });
    // clang-format on
}

int WebPage::stopApp() {
    // clang-format off
    return answer([] {
        Player::stop();
        return revision;
    });
    // clang-format on
}

void WebPage::inspect(double request, const char* path) {
    // clang-format off
    ConsoleRelay::requestInspection(debug::Inspector::splitPath(path), [request](bool ok, const core::Json& result) {
        completeRequest(request, ok, result);
    });
    // clang-format on
}

void WebPage::reportReloaded(const core::Json& report) {
    frameReports.push_back(report);
    haylen_js_reloaded(report.dump().c_str());
}

void WebPage::setVisible(bool visible) {
    Player::handleEvent({.type = visible ? Event::Type::Resumed : Event::Type::Suspended});
}

// The page usually hides before it goes away, which already sent the app to the background, so its files are persisted once more in case anything changed since.
void WebPage::hide() {
    Player::handleEvent({.type = Event::Type::Suspended});
    Services::persistUserData();
}

void WebPage::setOnline(bool online) {
    Player::handleEvent({.type = Event::Type::NetworkChanged, .online = online});
}

void WebPage::setTheme(bool dark) {
    NativeHost::getSystemState().setTheme(dark ? Theme::Dark : Theme::Light);
}

void WebPage::setTimeZone(const char* zone) {
    NativeHost::getSystemState().setTimeZone(zone);
}

void WebPage::setLanguages(const char* json) {
    NativeHost::getSystemState().setLanguages(core::Json::parse(json).get<std::vector<std::string>>());
}

// A full battery on mains power does not charge, as on the other platforms.
void WebPage::setBattery(double level, bool charging, bool full) {
    Battery battery{.level = static_cast<float>(level)};
    if (full) {
        battery.state = Battery::State::Full;
    } else if (charging) {
        battery.charging = true;
        battery.state = Battery::State::Charging;
    } else {
        battery.state = Battery::State::Discharging;
    }

    NativeHost::getSystemState().setBattery(battery);
}

// The browser tells nothing about screen readers, so the page never reports one.
void WebPage::setAccessibility(double textScale, bool reducedMotion, bool highContrast, bool reducedTransparency) {
    NativeHost::getSystemState().setAccessibility({.textScale = static_cast<float>(textScale), .reducedMotion = reducedMotion, .highContrast = highContrast, .reducedTransparency = reducedTransparency, .screenReader = false});
}

std::vector<std::vector<std::byte>> WebPage::readBuffers(const std::uint32_t* table, int count) {
    std::vector<std::vector<std::byte>> buffers;
    buffers.reserve(static_cast<std::size_t>(count));
    for (int index = 0; index < count; ++index) {
        const auto* bytes = reinterpret_cast<const std::byte*>(static_cast<std::uintptr_t>(table[index * 2]));
        buffers.emplace_back(bytes, bytes + table[index * 2 + 1]);
    }

    return buffers;
}

void WebPage::resolve(double call, bool ok, const char* json, const std::uint32_t* buffers, int count) {
    BridgeRelay::resolve(static_cast<std::uint64_t>(call), ok, json, readBuffers(buffers, count));
}

void WebPage::runConsoleLine(double id, const char* line) {
    ConsoleRelay::submit(line, [id](const debug::Console::Result& result) { completeRequest(id, true, {{"ok", result.ok}, {"output", result.output}}); });
}

void WebPage::emit(const char* event, const char* json, const std::uint32_t* buffers, int count, int flags) {
    const Bridge::EmitOptions options{.retain = (flags & HAYLEN_NATIVE_EMIT_RETAIN) != 0, .batched = (flags & HAYLEN_NATIVE_EMIT_BATCHED) != 0};
    BridgeRelay::emitOrKeep({.name = event, .json = json, .buffers = readBuffers(buffers, count), .options = options});
}

void WebPage::finishScreen(double id, bool ok, const char* json, const std::uint32_t* buffers, int count) {
    ScreenRelay::finish(static_cast<std::uint64_t>(id), ok, json, readBuffers(buffers, count));
}

void WebPage::restoreScreen(const char* plugin, const char* screen, const char* state, bool ok, const char* json, const std::uint32_t* buffers, int count) {
    ScreenRelay::restore(plugin, screen, state, ok, json, readBuffers(buffers, count));
}

std::vector<std::uint32_t> WebPage::describeBuffers(std::span<const std::vector<std::byte>> buffers) {
    std::vector<std::uint32_t> table;
    table.reserve(buffers.size() * 2);
    for (const std::vector<std::byte>& buffer : buffers) {
        table.push_back(static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(buffer.data())));
        table.push_back(static_cast<std::uint32_t>(buffer.size()));
    }

    return table;
}

void* WebPage::openVideoStream(const char* plugin, const char* name) {
    try {
        return PluginStreams::openVideo(plugin, name, VideoStream::Format::Rgba8, 0, 0).get();
    } catch (const std::exception& error) {
        lastError = error.what();
        return nullptr;
    }
}

int WebPage::pushVideoFrame(void* stream, const std::uint8_t* pixels, int width, int height, double timestamp) {
    // clang-format off
    return answer([&] {
        static_cast<VideoStream*>(stream)->push(reinterpret_cast<const std::byte*>(pixels), width, height, static_cast<std::size_t>(width) * 4, timestamp);
        return 1;
    });
    // clang-format on
}

void* WebPage::openAudioStream(const char* plugin, const char* name, int sampleRate, int channels, int capacityFrames) {
    try {
        if (sampleRate <= 0 || channels <= 0 || capacityFrames <= 0) {
            throw std::invalid_argument("An audio stream needs a sample rate, channels and room for at least one frame.");
        }

        return PluginStreams::openAudio(plugin, name, static_cast<std::uint32_t>(sampleRate), static_cast<std::uint32_t>(channels), AudioStream::Format::Float32, static_cast<std::size_t>(capacityFrames)).get();
    } catch (const std::exception& error) {
        lastError = error.what();
        return nullptr;
    }
}

int WebPage::pushAudioFrames(void* stream, const float* samples, int frames) {
    // clang-format off
    return answer([&] {
        AudioStream& target = *static_cast<AudioStream*>(stream);
        return static_cast<int>(target.push(std::span(samples, static_cast<std::size_t>(frames) * target.getChannels())));
    });
    // clang-format on
}

int WebPage::getAudioStreamRoom(void* stream) noexcept {
    const AudioStream& target = *static_cast<AudioStream*>(stream);
    return static_cast<int>(target.getCapacity() - target.getBufferedFrames());
}

core::Json WebPage::getFrameStatistics(core::Engine& engine) {
    const debug::Profiler& profiler = engine.getProfiler();
    const graphics2d::Renderer::Stats& render = engine.getRenderer2D().getStats();
    const double average = profiler.getAverageFrameMilliseconds();
    core::Json scopes = core::Json::array();
    for (const debug::ProfileSample& sample : profiler.getLastFrame()) {
        scopes.push_back({{"name", sample.name}, {"milliseconds", sample.milliseconds}, {"calls", sample.calls}, {"depth", sample.depth}});
    }

    return {
        {"fps", profiler.getFramesPerSecond()}, {"frameMilliseconds", profiler.getLastFrameMilliseconds()}, {"averageMilliseconds", average}, {"drawCalls", render.drawCalls}, {"sprites", render.sprites}, {"vertices", render.vertices}, {"textureSwitches", render.textureSwitches}, {"uploadedBytes", render.uploadedBytes}, {"assets", engine.getAssets().getCachedCount()}, {"voices", engine.getAudio().getVoiceCount()}, {"scopes", std::move(scopes)},
    };
}

} // namespace haylen::platform

// Entry points for the page, called through `Module.haylen` in `platform/web/haylen-runtime.js`.
extern "C" {

EMSCRIPTEN_KEEPALIVE void haylen_web_answer_gated_call(double id, const char* json) {
    haylen::platform::WebEntry::guard("haylen_web_answer_gated_call", [&] { haylen::platform::WebPage::answerGatedCall(id, json); });
}

EMSCRIPTEN_KEEPALIVE const char* haylen_web_last_error() {
    return haylen::platform::WebPage::getLastError();
}

EMSCRIPTEN_KEEPALIVE void haylen_web_resolve(double call, int ok, const char* json, const std::uint32_t* buffers, int count) {
    haylen::platform::WebEntry::guard("haylen_web_resolve", [&] { haylen::platform::WebPage::resolve(call, ok != 0, json, buffers, count); });
}

EMSCRIPTEN_KEEPALIVE void haylen_web_open_link(const char* url) {
    haylen::platform::WebEntry::guard("haylen_web_open_link", [&] { haylen::platform::LinkRelay::receive(url); });
}

EMSCRIPTEN_KEEPALIVE void haylen_web_emit(const char* event, const char* json, const std::uint32_t* buffers, int count, int flags) {
    haylen::platform::WebEntry::guard("haylen_web_emit", [&] { haylen::platform::WebPage::emit(event, json, buffers, count, flags); });
}

EMSCRIPTEN_KEEPALIVE void haylen_web_finish_screen(double id, int ok, const char* json, const std::uint32_t* buffers, int count) {
    haylen::platform::WebEntry::guard("haylen_web_finish_screen", [&] { haylen::platform::WebPage::finishScreen(id, ok != 0, json, buffers, count); });
}

EMSCRIPTEN_KEEPALIVE void haylen_web_restore_screen(const char* plugin, const char* screen, const char* state, int ok, const char* json, const std::uint32_t* buffers, int count) {
    haylen::platform::WebEntry::guard("haylen_web_restore_screen", [&] { haylen::platform::WebPage::restoreScreen(plugin, screen, state, ok != 0, json, buffers, count); });
}

EMSCRIPTEN_KEEPALIVE void* haylen_web_open_video_stream(const char* plugin, const char* name) {
    return haylen::platform::WebPage::openVideoStream(plugin, name);
}

EMSCRIPTEN_KEEPALIVE int haylen_web_push_video_frame(void* stream, const std::uint8_t* pixels, int width, int height, double timestamp) {
    return haylen::platform::WebPage::pushVideoFrame(stream, pixels, width, height, timestamp);
}

EMSCRIPTEN_KEEPALIVE void* haylen_web_open_audio_stream(const char* plugin, const char* name, int sampleRate, int channels, int capacityFrames) {
    return haylen::platform::WebPage::openAudioStream(plugin, name, sampleRate, channels, capacityFrames);
}

EMSCRIPTEN_KEEPALIVE int haylen_web_push_audio_frames(void* stream, const float* samples, int frames) {
    return haylen::platform::WebPage::pushAudioFrames(stream, samples, frames);
}

EMSCRIPTEN_KEEPALIVE int haylen_web_audio_stream_room(void* stream) {
    return haylen::platform::WebPage::getAudioStreamRoom(stream);
}

EMSCRIPTEN_KEEPALIVE void haylen_web_reserve_insets(const char* key, float left, float top, float right, float bottom) {
    haylen::platform::WebEntry::guard("haylen_web_reserve_insets", [&] { haylen::platform::NativeHost::getNativeViews().reserveInsets(key, {.left = left, .top = top, .right = right, .bottom = bottom}); });
}

EMSCRIPTEN_KEEPALIVE void haylen_web_release_insets(const char* key) {
    haylen::platform::WebEntry::guard("haylen_web_release_insets", [&] { haylen::platform::NativeHost::getNativeViews().releaseInsets(key); });
}

EMSCRIPTEN_KEEPALIVE void haylen_web_cover_app() {
    haylen::platform::WebEntry::guard("haylen_web_cover_app", [&] { haylen::platform::NativeHost::getNativeViews().coverApp(); });
}

EMSCRIPTEN_KEEPALIVE void haylen_web_uncover_app() {
    haylen::platform::WebEntry::guard("haylen_web_uncover_app", [&] { haylen::platform::NativeHost::getNativeViews().uncoverApp(); });
}

EMSCRIPTEN_KEEPALIVE int haylen_web_edit(const char* operations, const std::uint32_t* buffers, int count) {
    return haylen::platform::WebPage::edit(operations, buffers, count);
}

EMSCRIPTEN_KEEPALIVE int haylen_web_clear_files() {
    return haylen::platform::WebPage::clearFiles();
}

EMSCRIPTEN_KEEPALIVE int haylen_web_apply_changes(double request) {
    return haylen::platform::WebPage::applyChanges(request);
}

EMSCRIPTEN_KEEPALIVE int haylen_web_run_files(double request) {
    return haylen::platform::WebPage::runFiles(request);
}

EMSCRIPTEN_KEEPALIVE int haylen_web_load_zip(double request, const std::uint8_t* bytes, int size) {
    return haylen::platform::WebPage::loadZip(request, bytes, size);
}

EMSCRIPTEN_KEEPALIVE int haylen_web_stop() {
    return haylen::platform::WebPage::stopApp();
}

EMSCRIPTEN_KEEPALIVE void haylen_web_dispose() {
    haylen::platform::WebEntry::guard("haylen_web_dispose", [&] { haylen::platform::Player::end(); });
}

EMSCRIPTEN_KEEPALIVE void haylen_web_inspect(double request, const char* path) {
    haylen::platform::WebEntry::guard("haylen_web_inspect", [&] { haylen::platform::WebPage::inspect(request, path); });
}

EMSCRIPTEN_KEEPALIVE void haylen_web_console(double id, const char* line) {
    haylen::platform::WebEntry::guard("haylen_web_console", [&] { haylen::platform::WebPage::runConsoleLine(id, line); });
}

EMSCRIPTEN_KEEPALIVE void haylen_web_set_paused(int paused) {
    haylen::platform::WebEntry::guard("haylen_web_set_paused", [&] { haylen::platform::Player::setPaused(paused != 0); });
}

EMSCRIPTEN_KEEPALIVE int haylen_web_paused() {
    return haylen::platform::Player::isPaused() ? 1 : 0;
}

EMSCRIPTEN_KEEPALIVE void haylen_web_visibility(int visible) {
    haylen::platform::WebEntry::guard("haylen_web_visibility", [&] { haylen::platform::WebPage::setVisible(visible != 0); });
}

EMSCRIPTEN_KEEPALIVE void haylen_web_page_hidden() {
    haylen::platform::WebEntry::guard("haylen_web_page_hidden", [&] { haylen::platform::WebPage::hide(); });
}

EMSCRIPTEN_KEEPALIVE void haylen_web_network(int online) {
    haylen::platform::WebEntry::guard("haylen_web_network", [&] { haylen::platform::WebPage::setOnline(online != 0); });
}

EMSCRIPTEN_KEEPALIVE void haylen_web_theme(int dark) {
    haylen::platform::WebEntry::guard("haylen_web_theme", [&] { haylen::platform::WebPage::setTheme(dark != 0); });
}

EMSCRIPTEN_KEEPALIVE void haylen_web_languages(const char* json) {
    haylen::platform::WebEntry::guard("haylen_web_languages", [&] { haylen::platform::WebPage::setLanguages(json); });
}

EMSCRIPTEN_KEEPALIVE void haylen_web_accessibility_action(double element, int action) {
    haylen::platform::WebEntry::guard("haylen_web_accessibility_action", [&] { haylen::platform::AccessibilityRelay::perform({.element = static_cast<std::uint64_t>(element), .action = static_cast<haylen::platform::AccessibilityElement::Action>(action)}); });
}

EMSCRIPTEN_KEEPALIVE void haylen_web_accessibility(double textScale, int reducedMotion, int highContrast, int reducedTransparency) {
    haylen::platform::WebEntry::guard("haylen_web_accessibility", [&] { haylen::platform::WebPage::setAccessibility(textScale, reducedMotion != 0, highContrast != 0, reducedTransparency != 0); });
}

EMSCRIPTEN_KEEPALIVE void haylen_web_battery(double level, int charging, int full) {
    haylen::platform::WebEntry::guard("haylen_web_battery", [&] { haylen::platform::WebPage::setBattery(level, charging != 0, full != 0); });
}

EMSCRIPTEN_KEEPALIVE void haylen_web_time_zone(const char* zone) {
    haylen::platform::WebEntry::guard("haylen_web_time_zone", [&] { haylen::platform::WebPage::setTimeZone(zone); });
}

EMSCRIPTEN_KEEPALIVE void haylen_web_text_edited(double field, double revision, const char* text, int selectionStart, int selectionEnd, int compositionStart, int compositionEnd) {
    haylen::platform::WebEntry::guard("haylen_web_text_edited", [&] { haylen::platform::WebTextInput::receiveEdit(field, revision, text, selectionStart, selectionEnd, compositionStart, compositionEnd); });
}

EMSCRIPTEN_KEEPALIVE void haylen_web_text_action(double field, int action) {
    haylen::platform::WebEntry::guard("haylen_web_text_action", [&] { haylen::platform::WebTextInput::receiveAction(field, action); });
}

EMSCRIPTEN_KEEPALIVE void haylen_web_keyboard(float x, float y, float width, float height) {
    haylen::platform::WebEntry::guard("haylen_web_keyboard", [&] { haylen::platform::WebTextInput::receiveKeyboard(x, y, width, height); });
}
}
