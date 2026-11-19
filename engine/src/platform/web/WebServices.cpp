#include "platform/web/WebServices.hpp"

#include <emscripten/emscripten.h>

#include <array>
#include <cstdint>
#include <cstdlib>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "content/ReleasePackage.hpp"
#include "haylen/content/Bootstrap.hpp"
#include "haylen/core/Json.hpp"
#include "haylen/core/Log.hpp"
#include "haylen/io/Package.hpp"
#include "platform/NativeWindow.hpp"
#include "platform/web/BrowserAudioOutput.hpp"
#include "platform/web/WebDisplays.hpp"
#include "platform/web/WebPage.hpp"
#include "platform/web/WebStoreFiles.hpp"
#include "platform/web/WebVideoDecoder.hpp"

// The page side lives in `platform/web/haylen-page.js` and the engine side in `platform/web/haylen-runtime.js`, which define `Module.haylen` before the runtime starts.

// clang-format off
EM_JS(void, haylen_js_dispatch, (double call, const char* method, const char* params, const uint32_t* buffers, int count), {
    Module.haylen.dispatch(call, UTF8ToString(method), UTF8ToString(params), Module.haylen.readBuffers(buffers, count));
});

EM_JS(void, haylen_js_cancel, (double call), {
    Module.haylen.cancel(call);
});

EM_JS(void, haylen_js_open_screen, (double id, const char* plugin, const char* screen, const char* params, const uint32_t* buffers, int count, const char* state), {
    Module.haylen.openScreen(id, UTF8ToString(plugin), UTF8ToString(screen), UTF8ToString(params), Module.haylen.readBuffers(buffers, count), UTF8ToString(state));
});

EM_JS(void, haylen_js_cancel_screen, (double id), {
    Module.haylen.cancelScreen(id);
});

EM_JS(void, haylen_js_error, (const char* json), {
    Module.haylen.reportError(JSON.parse(UTF8ToString(json)));
});

EM_JS(char*, haylen_js_native_plugins, (), {
    return stringToNewUTF8(JSON.stringify(Module.haylen.nativePlugins()));
});

EM_JS(void, haylen_js_log, (int level, const char* message), {
    Module.haylen.reportLog(level, UTF8ToString(message));
});

EM_JS(void, haylen_js_safe_area, (float* insets), {
    const values = Module.haylen.safeAreaInsets();
    for (let index = 0; index < 4; ++index) {
        HEAPF32[(insets >> 2) + index] = values[index];
    }
});

EM_JS(int, haylen_js_fold, (float* values), {
    const fold = Module.haylen.fold();
    if (!fold) {
        return 0;
    }

    for (let index = 0; index < 8; ++index) {
        HEAPF32[(values >> 2) + index] = fold[index];
    }

    return 1;
});

EM_JS(void, haylen_js_persist, (), {
    Module.haylen.persist();
});

EM_JS(void, haylen_js_shutdown, (), {
    Module.haylen.reportShutdown();
});

EM_JS(char*, haylen_js_user_data_root, (), {
    return stringToNewUTF8(Module.haylen.userDataRoot);
});

EM_JS(char*, haylen_js_user_data_refusal, (), {
    return stringToNewUTF8(Module.haylen.userDataRefusal);
});

EM_JS(int, haylen_js_orientation, (), {
    return Module.haylen.orientation();
});

EM_JS(void, haylen_js_lock_orientation, (int value), {
    Module.haylen.lockOrientation(value);
});

EM_JS(char*, haylen_js_system_info, (), {
    return stringToNewUTF8(Module.haylen.systemInfo());
});

EM_JS(char*, haylen_js_languages, (), {
    return stringToNewUTF8(Module.haylen.languages());
});

EM_JS(void, haylen_js_update_accessibility, (const char* tree), {
    Module.haylen.accessibility.update(UTF8ToString(tree));
});

EM_JS(void, haylen_js_announce, (const char* text, int interrupt), {
    Module.haylen.accessibility.announce(UTF8ToString(text), interrupt > 0);
});

EM_JS(char*, haylen_js_release_address, (), {
    return stringToNewUTF8(Module.haylen.releaseAddress || "");
});
// clang-format on

namespace haylen::platform {

port::Services WebServices::create() {
    const auto services = std::make_shared<WebServices>();
    return {.platform = services, .displays = services, .screen = services, .bridge = services, .accessibility = services, .video = services, .audio = std::make_shared<audio::BrowserAudioOutput>()};
}

std::string_view WebServices::getName() const noexcept {
    return "web";
}

// The languages are known before the first app starts, so it never starts in a language that the page then changes.
void WebServices::initialize() {
    // clang-format off
    core::Log::addListener([](core::Log::Level level, std::string_view message) {
        haylen_js_log(static_cast<int>(level), std::string(message).c_str());
    });
    // clang-format on
    char* languages = haylen_js_languages();
    WebPage::setLanguages(languages);
    std::free(languages);
}

// The page that disposes of the runtime learns that the player ended, so it closes what the browser keeps for it.
void WebServices::shutdown() noexcept {
    haylen_js_shutdown();
}

// Script errors may quote bytes that are not UTF-8, which reach the page replaced instead of failing the report.
void WebServices::reportError(const core::Json& report) {
    haylen_js_error(report.dump(-1, ' ', false, core::Json::error_handler_t::replace).c_str());
}

// The plugins with a web part are the ones whose context the page created.
std::vector<std::string> WebServices::getNativePlugins() {
    char* json = haylen_js_native_plugins();
    const core::Json ids = core::Json::parse(json);
    std::free(json);
    return ids.get<std::vector<std::string>>();
}

// A page holds the manifests and the app domain of its release, and the shards of its content stay next to the page, where downloads read them.
std::shared_ptr<io::Package> WebServices::openBundledPackage() {
    char* address = haylen_js_release_address();
    std::string remote(address);
    std::free(address);
    // clang-format off
    return content::ReleasePackage::openBundled(io::Package::openDirectory("/app"), content::Bootstrap::find(), [](std::string_view) { return std::make_unique<WebStoreFiles>(); }, std::move(remote));
    // clang-format on
}

// The page mounts IndexedDB at the root of the user data, `/persistent` or the folder of its storage namespace, and loads it before the runtime starts.
std::filesystem::path WebServices::getUserDataDirectory(std::string_view identifier) {
    char* root = haylen_js_user_data_root();
    std::filesystem::path folder(root);
    std::free(root);
    return folder / std::string(identifier);
}

void WebServices::persistUserData() {
    haylen_js_persist();
}

// The user data of a page belongs to the one tab of the site that holds its lock, which the page takes before the runtime starts.
std::string WebServices::getUserDataRefusal() {
    char* refusal = haylen_js_user_data_refusal();
    std::string sentence(refusal);
    std::free(refusal);
    return sentence;
}

// The canvas is the window, and the page gives it no desktop.
math::Rect WebServices::getWindowFrame() {
    return {0.0F, 0.0F, NativeWindow::getFramebufferSize().x / NativeWindow::getScale(), NativeWindow::getFramebufferSize().y / NativeWindow::getScale()};
}

std::vector<Display> WebServices::getDisplays() {
    return WebDisplays::getDisplays();
}

std::string WebServices::getWindowDisplayId() {
    return WebDisplays::getWindowDisplayId();
}

bool WebServices::hasUnlistedDisplays() {
    return WebDisplays::hasUnlistedDisplays();
}

void WebServices::requestEveryScreen(std::function<void(SystemAnswer answer)> callback) {
    WebPage::requestEveryScreen(std::move(callback));
}

math::Insets WebServices::getSafeAreaInsets() {
    std::array<float, 4> insets{};
    haylen_js_safe_area(insets.data());
    return {.left = insets[0], .top = insets[1], .right = insets[2], .bottom = insets[3]};
}

bool WebServices::hasPointerDevice() const noexcept {
    return true;
}

Orientation WebServices::getOrientation() {
    return static_cast<Orientation>(haylen_js_orientation());
}

// Browsers lock the screen orientation only where the Screen Orientation API allows it, which usually means a fullscreen page on a phone.
void WebServices::lockOrientation(Orientation value) {
    haylen_js_lock_orientation(static_cast<int>(value));
}

// The status bar of a phone belongs to the browser, which shows it above the page.
void WebServices::setStatusBar(Window::StatusBar) {}

// A page has no say over the swipes that the system and the browser take at the edges of the screen.
void WebServices::setSystemGestures(Window::SystemGestures) {}

float WebServices::getStatusBarHeight() {
    return 0.0F;
}

// The page reads the segments of the viewport, which a browser splits at the fold of the device, and the posture of the device.
std::optional<Fold> WebServices::getFold() {
    std::array<float, 8> values{};
    if (haylen_js_fold(values.data()) == 0) {
        return std::nullopt;
    }

    return Fold{.bounds = math::Rect::fromMinMax({values[0], values[1]}, {values[2], values[3]}), .axis = values[4] != 0.0F ? Fold::Axis::Vertical : Fold::Axis::Horizontal, .state = values[5] != 0.0F ? Fold::State::HalfOpened : Fold::State::Flat, .separating = values[6] != 0.0F, .occluding = values[7] != 0.0F};
}

// Browsers tell the insets that cutouts take through `env(safe-area-inset-*)`, never the shapes of the cutouts.
void WebServices::getCutouts(std::vector<math::Rect>& cutouts) {
    cutouts.clear();
}

// The page acquires its own reference to every view while the dispatch runs, since the bridge holds the views until it returns.
void WebServices::dispatch(std::uint64_t id, std::string_view method, std::string_view paramsJson, std::span<const std::vector<std::byte>> buffers, std::span<const varn::dataview::DataView>) {
    const std::vector<std::uint32_t> table = WebPage::describeBuffers(buffers);
    haylen_js_dispatch(static_cast<double>(id), std::string(method).c_str(), std::string(paramsJson).c_str(), table.data(), static_cast<int>(buffers.size()));
}

void WebServices::cancel(std::uint64_t id) {
    haylen_js_cancel(static_cast<double>(id));
}

// Browsers limit what pages learn about the device on purpose, so most values stay empty, and the page reads the rest before the runtime starts.
SystemInfo WebServices::getSystemInfo() {
    char* text = haylen_js_system_info();
    const core::Json details = core::Json::parse(text);
    std::free(text);
    return {
        .os = SystemInfo::Os::Web,
        .osVersion = details.value("osVersion", std::string()),
        .deviceModel = details.value("deviceModel", std::string()),
        .deviceKind = SystemInfo::DeviceKind::Browser,
        .cpuCores = details.value("cpuCores", 0),
        .memoryBytes = details.value("memoryBytes", std::uint64_t{0}),
    };
}

// Browsers never tell whether a screen reader runs, so the page keeps its mirror of the UI current all the time.
bool WebServices::isTreeWanted() {
    return true;
}

void WebServices::update(std::span<const AccessibilityElement> elements, std::uint64_t focused) {
    haylen_js_update_accessibility(AccessibilityElement::writeTree(elements, focused).c_str());
}

void WebServices::announce(std::string_view text, bool interrupt) {
    haylen_js_announce(std::string(text).c_str(), interrupt ? 1 : 0);
}

std::unique_ptr<VideoDecoder> WebServices::openVideo(std::shared_ptr<io::PackageReader> file, std::string_view path) {
    return std::make_unique<WebVideoDecoder>(*file, path);
}

// The page opens the screen before the call returns, one frame after the request, so a popup opens while the tap that asked for it still counts as an activation of the user.
void WebServices::openScreen(const ScreenRequest& request) {
    const std::vector<std::uint32_t> table = WebPage::describeBuffers(request.params.buffers);
    haylen_js_open_screen(static_cast<double>(request.id), request.plugin.c_str(), request.screen.c_str(), request.params.json.dump().c_str(), table.data(), static_cast<int>(request.params.buffers.size()), request.state.dump().c_str());
}

void WebServices::cancelScreen(std::uint64_t id) {
    haylen_js_cancel_screen(static_cast<double>(id));
}

} // namespace haylen::platform
