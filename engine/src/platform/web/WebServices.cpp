#include "platform/Services.hpp"

#include <emscripten/emscripten.h>
#include <emscripten/html5.h>

#include <array>
#include <cstdint>
#include <cstdlib>
#include <span>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include "content/ReleasePackage.hpp"
#include "haylen/content/Bootstrap.hpp"
#include "haylen/core/Json.hpp"
#include "haylen/core/Log.hpp"
#include "haylen/io/Package.hpp"
#include "platform/DialogRelay.hpp"
#include "platform/web/WebDialogJson.hpp"
#include "platform/web/WebPage.hpp"
#include "platform/web/WebTextInput.hpp"
#include "sokol_app.h"

// The page side lives in `platform/web/haylen-runtime.js`, which defines `Module.haylen` before the runtime starts.

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

EM_JS(void, haylen_js_persist, (), {
    Module.haylen.persist();
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

EM_JS(int, haylen_js_open_url, (const char* url), {
    return Module.haylen.openUrl(UTF8ToString(url)) ? 1 : 0;
});

EM_JS(void, haylen_js_vibrate, (float seconds), {
    Module.haylen.vibrate(seconds);
});

EM_JS(void, haylen_js_show_dialog, (double id, const char* json, const uint8_t* data, int size), {
    Module.haylen.showDialog(id, JSON.parse(UTF8ToString(json)), HEAPU8.slice(data, data + size));
});

EM_JS(void, haylen_js_cancel_dialog, (double id), {
    Module.haylen.cancelDialog(id);
});
// clang-format on

namespace haylen::platform {

std::string_view Services::getName() noexcept {
    return "web";
}

void Services::initialize() {
    // clang-format off
    core::Log::addListener([](core::Log::Level level, std::string_view message) {
        haylen_js_log(static_cast<int>(level), std::string(message).c_str());
    });
    // clang-format on
}

void Services::shutdown() noexcept {}

// Script errors may quote bytes that are not UTF-8, which reach the page replaced instead of failing the report.
void Services::reportError(const core::Json& report) {
    haylen_js_error(report.dump(-1, ' ', false, core::Json::error_handler_t::replace).c_str());
}

// The plugins with a web part are the ones whose context the page created.
std::vector<std::string> Services::getNativePlugins() {
    char* json = haylen_js_native_plugins();
    const core::Json ids = core::Json::parse(json);
    std::free(json);
    return ids.get<std::vector<std::string>>();
}

std::shared_ptr<io::Package> Services::openBundledPackage() {
    return content::ReleasePackage::openBundled(io::Package::openDirectory("/app"), content::Bootstrap::find());
}

std::filesystem::path Services::getUserDataDirectory(std::string_view identifier) {
    // The page mounts IndexedDB at `/persistent` and loads it before the runtime starts.
    return std::filesystem::path("/persistent") / std::string(identifier);
}

void Services::persistUserData() {
    haylen_js_persist();
}

bool Services::hasDesktop() noexcept {
    return false;
}

void Services::setWindowStyle(const WindowStyle&) {}

// The canvas is the window, and the page gives it no desktop, so the canvas is also the only monitor.
math::Rect Services::getWindowFrame() {
    return {0.0F, 0.0F, sapp_widthf() / sapp_dpi_scale(), sapp_heightf() / sapp_dpi_scale()};
}

void Services::setWindowFrame(const math::Rect&) {}

std::vector<Monitor> Services::getMonitors() {
    const math::Rect screen = getWindowFrame();
    return {{.name = "canvas", .bounds = screen, .workArea = screen, .scale = sapp_dpi_scale(), .primary = true}};
}

void Services::setMousePassthrough(Window::Passthrough, std::span<const math::Polygon::Outline>) {}

void Services::startWindowDrag() {}

void Services::watchWindow() {}

void Services::updateWindow() {}

math::Insets Services::getSafeAreaInsets() {
    std::array<float, 4> insets{};
    haylen_js_safe_area(insets.data());
    return {.left = insets[0], .top = insets[1], .right = insets[2], .bottom = insets[3]};
}

bool Services::hasPointerDevice() noexcept {
    return true;
}

void Services::pollGamepads(std::span<input::GamepadState> gamepads) {
    const bool sampled = emscripten_sample_gamepad_data() == EMSCRIPTEN_RESULT_SUCCESS;
    const int available = sampled ? emscripten_get_num_gamepads() : 0;
    for (std::size_t index = 0; index < gamepads.size(); ++index) {
        input::GamepadState& state = gamepads[index];
        EmscriptenGamepadEvent pad{};
        if (static_cast<int>(index) >= available || emscripten_get_gamepad_status(static_cast<int>(index), &pad) != EMSCRIPTEN_RESULT_SUCCESS || pad.connected == 0 || std::string_view(pad.mapping) != "standard") {
            state = {};
            continue;
        }

        // The standard mapping of the Gamepad API numbers buttons and axes the same way on every browser.
        constexpr std::array<std::pair<int, input::GamepadButton>, 15> kButtons{{{0, input::GamepadButton::South}, {1, input::GamepadButton::East}, {2, input::GamepadButton::West}, {3, input::GamepadButton::North}, {4, input::GamepadButton::LeftShoulder}, {5, input::GamepadButton::RightShoulder}, {8, input::GamepadButton::Back}, {9, input::GamepadButton::Start}, {10, input::GamepadButton::LeftStick}, {11, input::GamepadButton::RightStick}, {12, input::GamepadButton::DpadUp}, {13, input::GamepadButton::DpadDown}, {14, input::GamepadButton::DpadLeft}, {15, input::GamepadButton::DpadRight}, {16, input::GamepadButton::Guide}}};
        state.connected = true;
        state.name = pad.id;
        for (const auto& [source, button] : kButtons) {
            state.buttons[static_cast<std::size_t>(button)] = source < pad.numButtons && pad.digitalButton[source] != 0;
        }
        state.axes[static_cast<std::size_t>(input::GamepadAxis::LeftX)] = static_cast<float>(pad.axis[0]);
        state.axes[static_cast<std::size_t>(input::GamepadAxis::LeftY)] = static_cast<float>(pad.axis[1]);
        state.axes[static_cast<std::size_t>(input::GamepadAxis::RightX)] = static_cast<float>(pad.axis[2]);
        state.axes[static_cast<std::size_t>(input::GamepadAxis::RightY)] = static_cast<float>(pad.axis[3]);
        state.axes[static_cast<std::size_t>(input::GamepadAxis::LeftTrigger)] = static_cast<float>(pad.analogButton[6]);
        state.axes[static_cast<std::size_t>(input::GamepadAxis::RightTrigger)] = static_cast<float>(pad.analogButton[7]);
    }
}

Orientation Services::getOrientation() {
    return static_cast<Orientation>(haylen_js_orientation());
}

// Browsers lock the screen orientation only where the Screen Orientation API allows it, which usually means a fullscreen page on a phone.
void Services::lockOrientation(Orientation value) {
    haylen_js_lock_orientation(static_cast<int>(value));
}

TextInput& Services::getTextInput() {
    static WebTextInput& input = *new WebTextInput();
    return input;
}

void Services::dispatch(std::uint64_t id, std::string_view method, std::string_view paramsJson, std::span<const std::vector<std::byte>> buffers) {
    const std::vector<std::uint32_t> table = WebPage::describeBuffers(buffers);
    haylen_js_dispatch(static_cast<double>(id), std::string(method).c_str(), std::string(paramsJson).c_str(), table.data(), static_cast<int>(buffers.size()));
}

void Services::cancel(std::uint64_t id) {
    haylen_js_cancel(static_cast<double>(id));
}

// Browsers limit what pages learn about the device on purpose, so most values stay empty, and the page reads the rest before the runtime starts.
SystemInfo Services::getSystemInfo() {
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
        .locale = details.value("locale", std::string()),
        .languages = details.at("languages").get<std::vector<std::string>>(),
        .timeZone = details.value("timeZone", std::string()),
    };
}

// The page opens the url in a new tab at once, so the answer comes during the call.
void Services::openUrl(const std::string& url, std::function<void(bool opened)> callback) {
    callback(haylen_js_open_url(url.c_str()) != 0);
}

void Services::vibrate(float seconds) {
    haylen_js_vibrate(seconds);
}

// The page shows the dialog in the frame that asked for it, and titles that are not UTF-8 reach it replaced.
void Services::showDialog(std::uint64_t id, const DialogRequest& request, const std::filesystem::path& folder) {
    const auto show = [id](const core::Json& dialog, std::span<const std::uint8_t> data) { haylen_js_show_dialog(static_cast<double>(id), dialog.dump(-1, ' ', false, core::Json::error_handler_t::replace).c_str(), data.data(), static_cast<int>(data.size())); };
    if (const auto* message = std::get_if<DialogRequest::Message>(&request.dialog)) {
        show(WebDialogJson::describeMessage(*message), {});
    } else if (const auto* files = std::get_if<DialogRequest::OpenFiles>(&request.dialog)) {
        show(WebDialogJson::describeOpenFiles(*files, folder), {});
    } else if (const auto* save = std::get_if<DialogRequest::SaveFile>(&request.dialog)) {
        show(WebDialogJson::describeSaveFile(*save), save->data);
    } else {
        DialogRelay::resolve(id, {.failure = DialogResult::Failure{.code = DialogResult::Code::Unsupported, .message = "Browsers give pages no paths of folders, so the web opens no folders."}});
    }
}

void Services::cancelDialog(std::uint64_t id) {
    haylen_js_cancel_dialog(static_cast<double>(id));
}

// The page opens the screen before the call returns, one frame after the request, so a popup opens while the tap that asked for it still counts as an activation of the user.
void Services::openScreen(const ScreenRequest& request) {
    const std::vector<std::uint32_t> table = WebPage::describeBuffers(request.params.buffers);
    haylen_js_open_screen(static_cast<double>(request.id), request.plugin.c_str(), request.screen.c_str(), request.params.json.dump().c_str(), table.data(), static_cast<int>(request.params.buffers.size()), request.state.dump().c_str());
}

void Services::cancelScreen(std::uint64_t id) {
    haylen_js_cancel_screen(static_cast<double>(id));
}

HaylenNativeWindow Services::getNativeWindow() {
    return {};
}

} // namespace haylen::platform
