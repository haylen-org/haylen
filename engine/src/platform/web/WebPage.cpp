#include "platform/web/WebPage.hpp"

#include <emscripten/emscripten.h>

#include <cstdlib>
#include <vector>

#include "haylen/2d/graphics/Renderer.hpp"
#include "haylen/assets/Manager.hpp"
#include "haylen/audio/Mixer.hpp"
#include "haylen/core/AppConfig.hpp"
#include "haylen/core/Engine.hpp"
#include "haylen/debug/Profiler.hpp"
#include "haylen/io/MemoryPackage.hpp"
#include "haylen/io/Package.hpp"
#include "haylen/platform/Event.hpp"
#include "platform/BridgeRelay.hpp"
#include "platform/Services.hpp"
#include "platform/sokol/SokolRuntime.hpp"
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

// Entry points for the page, called through Module.haylen in platform/web/haylen-runtime.js.
extern "C" {

EMSCRIPTEN_KEEPALIVE const char* haylen_web_last_error() {
    return haylen::platform::WebPage::getLastError();
}

EMSCRIPTEN_KEEPALIVE void haylen_web_resolve(double call, int ok, const char* json) {
    haylen::platform::BridgeRelay::resolve(static_cast<std::uint64_t>(call), ok != 0, json);
}

EMSCRIPTEN_KEEPALIVE void haylen_web_emit(const char* event, const char* json) {
    haylen::platform::BridgeRelay::emit(event, json);
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
