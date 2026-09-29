#include "platform/Services.hpp"

#include <emscripten/emscripten.h>
#include <emscripten/html5.h>

#include <array>
#include <string>
#include <utility>

#include "haylen/core/Json.hpp"
#include "haylen/core/Log.hpp"
#include "haylen/io/Package.hpp"
#include "haylen/lua/Error.hpp"
#include "platform/web/WebTextInput.hpp"
#include "sokol_app.h"

// The page side lives in platform/web/haylen-runtime.js, which defines Module.haylen before the runtime starts.

// clang-format off
EM_JS(void, haylen_js_dispatch, (double call, const char* method, const char* params), {
    Module.haylen.dispatch(call, UTF8ToString(method), UTF8ToString(params));
});

EM_JS(void, haylen_js_cancel, (double call), {
    Module.haylen.cancel(call);
});

EM_JS(void, haylen_js_error, (const char* json), {
    Module.haylen.reportError(JSON.parse(UTF8ToString(json)));
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
void Services::reportError(const lua::Error& error) {
    haylen_js_error(error.toJson().dump(-1, ' ', false, core::Json::error_handler_t::replace).c_str());
}

std::shared_ptr<io::Package> Services::openBundledPackage() {
    return io::Package::openDirectory("/app");
}

std::filesystem::path Services::getUserDataDirectory(std::string_view identifier) {
    // The page mounts IndexedDB at /persistent and loads it before the runtime starts.
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

void Services::dispatch(std::uint64_t id, std::string_view method, std::string_view paramsJson) {
    haylen_js_dispatch(static_cast<double>(id), std::string(method).c_str(), std::string(paramsJson).c_str());
}

void Services::cancel(std::uint64_t id) {
    haylen_js_cancel(static_cast<double>(id));
}

} // namespace haylen::platform
