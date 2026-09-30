#include "platform/web/WebPage.hpp"

#include <emscripten/emscripten.h>

#include <cstdlib>
#include <exception>
#include <span>
#include <stdexcept>
#include <vector>

#include "haylen/2d/graphics/Renderer.hpp"
#include "haylen/assets/Manager.hpp"
#include "haylen/audio/Mixer.hpp"
#include "haylen/core/AppConfig.hpp"
#include "haylen/core/Engine.hpp"
#include "haylen/debug/Profiler.hpp"
#include "haylen/io/MemoryPackage.hpp"
#include "haylen/io/Package.hpp"
#include "haylen/platform/Battery.hpp"
#include "haylen/platform/Event.hpp"
#include "haylen/platform/PluginStreams.hpp"
#include "haylen/platform/Theme.hpp"
#include "haylen/platform/native/HaylenNative.h"
#include "platform/BridgeRelay.hpp"
#include "platform/DialogRelay.hpp"
#include "platform/ScreenRelay.hpp"
#include "platform/Services.hpp"
#include "platform/sokol/SokolHost.hpp"
#include "platform/sokol/SokolRuntime.hpp"
#include "platform/web/WebDialogJson.hpp"
#include "platform/web/WebTextInput.hpp"

// clang-format off
EM_JS(void, haylen_js_started, (const char* json), {
    Module.haylen.reportStarted(JSON.parse(UTF8ToString(json)));
});

EM_JS(void, haylen_js_stopped, (), {
    Module.haylen.reportStopped();
});

EM_JS(void, haylen_js_stats, (const char* json), {
    Module.haylen.reportStats(JSON.parse(UTF8ToString(json)));
});

EM_JS(char*, haylen_js_canvas_selector, (), {
    return stringToNewUTF8(Module.haylen.canvasSelector());
});
// clang-format on

namespace haylen::platform {

std::shared_ptr<io::MemoryPackage>& WebPage::editorPackage = *new std::shared_ptr<io::MemoryPackage>(std::make_shared<io::MemoryPackage>("editor"));
std::string& WebPage::lastError = *new std::string();
double WebPage::lastStats = 0.0;

template <typename Body> int WebPage::answer(Body&& body) {
    try {
        return body();
    } catch (const std::exception& error) {
        lastError = error.what();
        return -1;
    }
}

std::string WebPage::getCanvasSelector() {
    char* selector = haylen_js_canvas_selector();
    std::string result(selector);
    std::free(selector);
    return result;
}

void WebPage::reportStarted(const core::AppConfig& config) {
    lastStats = emscripten_get_now();
    haylen_js_started(core::Json{{"name", config.name}, {"identifier", config.identifier}, {"version", config.version}}.dump().c_str());
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

const char* WebPage::getLastError() noexcept {
    return lastError.c_str();
}

void WebPage::loadZip(const std::uint8_t* bytes, int size) {
    std::vector<std::uint8_t> archive(bytes, bytes + size);
    SokolRuntime::restart([&archive] { return io::Package::openZip(std::move(archive), "app.zip"); });
}

void WebPage::clearFiles() {
    editorPackage = std::make_shared<io::MemoryPackage>("editor");
}

int WebPage::setFile(const char* path, const std::uint8_t* bytes, int size) {
    // clang-format off
    return answer([&] {
        editorPackage->setFile(path, std::vector<std::uint8_t>(bytes, bytes + size));
        return 1;
    });
    // clang-format on
}

int WebPage::removeFile(const char* path) {
    return answer([&] { return editorPackage->removeFile(path) ? 1 : 0; });
}

void WebPage::runFiles() {
    SokolRuntime::restart(editorPackage);
}

int WebPage::reloadAsset(const char* path) {
    return answer([&] { return SokolRuntime::reloadAsset(path) ? 1 : 0; });
}

void WebPage::setVisible(bool visible) {
    SokolRuntime::handleEvent({.type = visible ? Event::Type::Resumed : Event::Type::Suspended});
}

// The page usually hides before it goes away, which already sent the app to the background, so its files are persisted once more in case anything changed since.
void WebPage::hide() {
    SokolRuntime::handleEvent({.type = Event::Type::Suspended});
    Services::persistUserData();
}

void WebPage::setOnline(bool online) {
    SokolRuntime::handleEvent({.type = Event::Type::NetworkChanged, .online = online});
}

void WebPage::setTheme(bool dark) {
    SokolHost::getSystemState().setTheme(dark ? Theme::Dark : Theme::Light);
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
    SokolHost::getSystemState().setBattery(battery);
}

void WebPage::resolveDialog(double id, const char* json) {
    DialogRelay::resolve(static_cast<std::uint64_t>(id), WebDialogJson::readAnswer(json));
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

void WebPage::emit(const char* event, const char* json, const std::uint32_t* buffers, int count, int flags) {
    BridgeRelay::emit(event, json, readBuffers(buffers, count), {.retain = (flags & HAYLEN_NATIVE_EMIT_RETAIN) != 0, .batched = (flags & HAYLEN_NATIVE_EMIT_BATCHED) != 0});
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

core::Json WebPage::getFrameStatistics(core::Engine& engine) {
    const debug::Profiler& profiler = engine.getProfiler();
    const graphics2d::Renderer::Stats& render = engine.getRenderer2D().getStats();
    const double average = profiler.getAverageFrameMilliseconds();
    core::Json scopes = core::Json::array();
    for (const debug::ProfileSample& sample : profiler.getLastFrame()) {
        scopes.push_back({{"name", sample.name}, {"milliseconds", sample.milliseconds}, {"calls", sample.calls}, {"depth", sample.depth}});
    }
    return {
        {"fps", average > 0.0 ? 1000.0 / average : 0.0}, {"frameMilliseconds", profiler.getLastFrameMilliseconds()}, {"averageMilliseconds", average}, {"drawCalls", render.drawCalls}, {"sprites", render.sprites}, {"vertices", render.vertices}, {"textureSwitches", render.textureSwitches}, {"uploadedBytes", render.uploadedBytes}, {"assets", engine.getAssets().getCachedCount()}, {"voices", engine.getAudio().getVoiceCount()}, {"scopes", std::move(scopes)},
    };
}

} // namespace haylen::platform

// Entry points for the page, called through `Module.haylen` in `platform/web/haylen-runtime.js`.
extern "C" {

EMSCRIPTEN_KEEPALIVE const char* haylen_web_last_error() {
    return haylen::platform::WebPage::getLastError();
}

EMSCRIPTEN_KEEPALIVE void haylen_web_resolve(double call, int ok, const char* json, const std::uint32_t* buffers, int count) {
    haylen::platform::WebPage::resolve(call, ok != 0, json, buffers, count);
}

EMSCRIPTEN_KEEPALIVE void haylen_web_emit(const char* event, const char* json, const std::uint32_t* buffers, int count, int flags) {
    haylen::platform::WebPage::emit(event, json, buffers, count, flags);
}

EMSCRIPTEN_KEEPALIVE void haylen_web_finish_screen(double id, int ok, const char* json, const std::uint32_t* buffers, int count) {
    haylen::platform::WebPage::finishScreen(id, ok != 0, json, buffers, count);
}

EMSCRIPTEN_KEEPALIVE void haylen_web_restore_screen(const char* plugin, const char* screen, const char* state, int ok, const char* json, const std::uint32_t* buffers, int count) {
    haylen::platform::WebPage::restoreScreen(plugin, screen, state, ok != 0, json, buffers, count);
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

EMSCRIPTEN_KEEPALIVE void haylen_web_reserve_insets(const char* key, float left, float top, float right, float bottom) {
    haylen::platform::SokolHost::getNativeViews().reserveInsets(key, {.left = left, .top = top, .right = right, .bottom = bottom});
}

EMSCRIPTEN_KEEPALIVE void haylen_web_release_insets(const char* key) {
    haylen::platform::SokolHost::getNativeViews().releaseInsets(key);
}

EMSCRIPTEN_KEEPALIVE void haylen_web_cover_app() {
    haylen::platform::SokolHost::getNativeViews().coverApp();
}

EMSCRIPTEN_KEEPALIVE void haylen_web_uncover_app() {
    haylen::platform::SokolHost::getNativeViews().uncoverApp();
}

EMSCRIPTEN_KEEPALIVE void haylen_web_load_zip(const std::uint8_t* bytes, int size) {
    haylen::platform::WebPage::loadZip(bytes, size);
}

EMSCRIPTEN_KEEPALIVE void haylen_web_clear_files() {
    haylen::platform::WebPage::clearFiles();
}

EMSCRIPTEN_KEEPALIVE int haylen_web_set_file(const char* path, const std::uint8_t* bytes, int size) {
    return haylen::platform::WebPage::setFile(path, bytes, size);
}

EMSCRIPTEN_KEEPALIVE int haylen_web_remove_file(const char* path) {
    return haylen::platform::WebPage::removeFile(path);
}

EMSCRIPTEN_KEEPALIVE void haylen_web_run_files() {
    haylen::platform::WebPage::runFiles();
}

EMSCRIPTEN_KEEPALIVE void haylen_web_restart() {
    haylen::platform::SokolRuntime::restart();
}

EMSCRIPTEN_KEEPALIVE void haylen_web_stop() {
    haylen::platform::SokolRuntime::stop();
}

EMSCRIPTEN_KEEPALIVE void haylen_web_set_paused(int paused) {
    haylen::platform::SokolRuntime::setPaused(paused != 0);
}

EMSCRIPTEN_KEEPALIVE int haylen_web_paused() {
    return haylen::platform::SokolRuntime::isPaused() ? 1 : 0;
}

EMSCRIPTEN_KEEPALIVE int haylen_web_reload_asset(const char* path) {
    return haylen::platform::WebPage::reloadAsset(path);
}

EMSCRIPTEN_KEEPALIVE void haylen_web_visibility(int visible) {
    haylen::platform::WebPage::setVisible(visible != 0);
}

EMSCRIPTEN_KEEPALIVE void haylen_web_page_hidden() {
    haylen::platform::WebPage::hide();
}

EMSCRIPTEN_KEEPALIVE void haylen_web_network(int online) {
    haylen::platform::WebPage::setOnline(online != 0);
}

EMSCRIPTEN_KEEPALIVE void haylen_web_theme(int dark) {
    haylen::platform::WebPage::setTheme(dark != 0);
}

EMSCRIPTEN_KEEPALIVE void haylen_web_battery(double level, int charging, int full) {
    haylen::platform::WebPage::setBattery(level, charging != 0, full != 0);
}

EMSCRIPTEN_KEEPALIVE void haylen_web_resolve_dialog(double id, const char* json) {
    haylen::platform::WebPage::resolveDialog(id, json);
}

EMSCRIPTEN_KEEPALIVE void haylen_web_text_edited(double field, double revision, const char* text, int selectionStart, int selectionEnd, int compositionStart, int compositionEnd) {
    haylen::platform::WebTextInput::receiveEdit(field, revision, text, selectionStart, selectionEnd, compositionStart, compositionEnd);
}

EMSCRIPTEN_KEEPALIVE void haylen_web_text_action(double field, int action) {
    haylen::platform::WebTextInput::receiveAction(field, action);
}

EMSCRIPTEN_KEEPALIVE void haylen_web_keyboard(float x, float y, float width, float height) {
    haylen::platform::WebTextInput::receiveKeyboard(x, y, width, height);
}
}
