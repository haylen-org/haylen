#include "platform/Services.hpp"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <knownfolders.h>
#include <shlobj.h>
#include <xinput.h>

#include <stdexcept>
#include <string>
#include <utility>

#include "haylen/core/Json.hpp"
#include "haylen/io/Package.hpp"
#include "platform/BridgeRelay.hpp"
#include "platform/ScreenRelay.hpp"
#include "platform/windows/WindowsDesktop.hpp"
#include "platform/windows/WindowsDialogs.hpp"
#include "platform/windows/WindowsSystem.hpp"
#include "platform/windows/WindowsTextInput.hpp"
#include "sokol_app.h"

namespace haylen::platform {

std::string_view Services::getName() noexcept {
    return "windows";
}

void Services::initialize() {
    WindowsDesktop::initialize();
    WindowsSystem::reportTheme();
    WindowsSystem::reportBattery();
}

void Services::shutdown() noexcept {}

void Services::reportError(const core::Json&) {}

// The platform loads no native plugins of its own, and native libraries declare theirs through `HaylenNativeApi`.
std::vector<std::string> Services::getNativePlugins() {
    return {};
}

std::shared_ptr<io::Package> Services::openBundledPackage() {
    std::wstring executable(MAX_PATH, L'\0');
    for (;;) {
        const DWORD length = GetModuleFileNameW(nullptr, executable.data(), static_cast<DWORD>(executable.size()));
        if (length < executable.size()) {
            executable.resize(length);
            break;
        }
        executable.resize(executable.size() * 2);
    }

    const std::filesystem::path directory = std::filesystem::path(executable).parent_path();
    if (std::filesystem::is_directory(directory / "app")) {
        return io::Package::open(directory / "app");
    }
    return io::Package::open(directory / "app.zip");
}

std::filesystem::path Services::getUserDataDirectory(std::string_view identifier) {
    PWSTR folder = nullptr;
    if (FAILED(SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, nullptr, &folder))) {
        CoTaskMemFree(folder);
        throw std::runtime_error("The application data folder could not be found.");
    }
    std::filesystem::path path(folder);
    CoTaskMemFree(folder);
    return path / std::string(identifier);
}

void Services::persistUserData() {}

bool Services::hasDesktop() noexcept {
    return true;
}

void Services::setWindowStyle(const WindowStyle& value) {
    WindowsDesktop::setStyle(value);
}

math::Rect Services::getWindowFrame() {
    return WindowsDesktop::getFrame();
}

void Services::setWindowFrame(const math::Rect& value) {
    WindowsDesktop::setFrame(value);
}

std::vector<Monitor> Services::getMonitors() {
    return WindowsDesktop::getMonitors();
}

void Services::setMousePassthrough(Window::Passthrough mode, std::span<const math::Polygon::Outline> regions) {
    WindowsDesktop::setPassthrough(mode, regions);
}

void Services::startWindowDrag() {
    WindowsDesktop::startDrag();
}

void Services::watchWindow() {
    WindowsDesktop::watch();
}

void Services::updateWindow() {
    WindowsDesktop::update();
}

math::Insets Services::getSafeAreaInsets() {
    return {};
}

bool Services::hasPointerDevice() noexcept {
    return true;
}

void Services::pollGamepads(std::span<input::GamepadState> gamepads) {
    const auto stick = [](SHORT value) { return value < 0 ? static_cast<float>(value) / 32768.0F : static_cast<float>(value) / 32767.0F; };
    for (std::size_t index = 0; index < gamepads.size(); ++index) {
        input::GamepadState& state = gamepads[index];
        XINPUT_STATE reading{};
        if (index >= XUSER_MAX_COUNT || XInputGetState(static_cast<DWORD>(index), &reading) != ERROR_SUCCESS) {
            state = {};
            continue;
        }

        const XINPUT_GAMEPAD& pad = reading.Gamepad;
        state.connected = true;
        state.name = "Xbox Controller";
        const auto set = [&state, &pad](input::GamepadButton button, WORD mask) { state.buttons[static_cast<std::size_t>(button)] = (pad.wButtons & mask) != 0; };
        set(input::GamepadButton::South, XINPUT_GAMEPAD_A);
        set(input::GamepadButton::East, XINPUT_GAMEPAD_B);
        set(input::GamepadButton::West, XINPUT_GAMEPAD_X);
        set(input::GamepadButton::North, XINPUT_GAMEPAD_Y);
        set(input::GamepadButton::LeftShoulder, XINPUT_GAMEPAD_LEFT_SHOULDER);
        set(input::GamepadButton::RightShoulder, XINPUT_GAMEPAD_RIGHT_SHOULDER);
        set(input::GamepadButton::Back, XINPUT_GAMEPAD_BACK);
        set(input::GamepadButton::Start, XINPUT_GAMEPAD_START);
        set(input::GamepadButton::LeftStick, XINPUT_GAMEPAD_LEFT_THUMB);
        set(input::GamepadButton::RightStick, XINPUT_GAMEPAD_RIGHT_THUMB);
        set(input::GamepadButton::DpadUp, XINPUT_GAMEPAD_DPAD_UP);
        set(input::GamepadButton::DpadDown, XINPUT_GAMEPAD_DPAD_DOWN);
        set(input::GamepadButton::DpadLeft, XINPUT_GAMEPAD_DPAD_LEFT);
        set(input::GamepadButton::DpadRight, XINPUT_GAMEPAD_DPAD_RIGHT);

        // XInput reports up as positive, while the engine follows the screen with down as positive.
        state.axes[static_cast<std::size_t>(input::GamepadAxis::LeftX)] = stick(pad.sThumbLX);
        state.axes[static_cast<std::size_t>(input::GamepadAxis::LeftY)] = -stick(pad.sThumbLY);
        state.axes[static_cast<std::size_t>(input::GamepadAxis::RightX)] = stick(pad.sThumbRX);
        state.axes[static_cast<std::size_t>(input::GamepadAxis::RightY)] = -stick(pad.sThumbRY);
        state.axes[static_cast<std::size_t>(input::GamepadAxis::LeftTrigger)] = static_cast<float>(pad.bLeftTrigger) / 255.0F;
        state.axes[static_cast<std::size_t>(input::GamepadAxis::RightTrigger)] = static_cast<float>(pad.bRightTrigger) / 255.0F;
    }
}

Orientation Services::getOrientation() {
    return Orientation::Landscape;
}

void Services::lockOrientation(Orientation) {}

TextInput& Services::getTextInput() {
    static WindowsTextInput& input = *new WindowsTextInput();
    return input;
}

// Windows has no handler registry in the language of the platform, so native libraries and C++ plugins answer the methods of apps.
void Services::dispatch(std::uint64_t id, std::string_view method, std::string_view, std::span<const std::vector<std::byte>>) {
    BridgeRelay::resolve(id, false, core::Json{{"message", "No native handler is registered for \"" + std::string(method) + "\"."}, {"code", "noHandler"}}.dump());
}

// Every call fails while it is made, so no call is ever pending here.
void Services::cancel(std::uint64_t) {}

SystemInfo Services::getSystemInfo() {
    return WindowsSystem::getInfo();
}

void Services::openUrl(const std::string& url, std::function<void(bool opened)> callback) {
    WindowsSystem::openUrl(url, std::move(callback));
}

void Services::vibrate(float) {}

// Picked files are files of the computer, which the app reads where they are.
void Services::showDialog(std::uint64_t id, const DialogRequest& request, const std::filesystem::path&) {
    WindowsDialogs::show(id, request);
}

void Services::cancelDialog(std::uint64_t id) {
    WindowsDialogs::cancel(id);
}

// Windows has no screen registry in the language of the platform, so native libraries open the screens of plugins, before the platform is asked.
void Services::openScreen(const ScreenRequest& request) {
    ScreenRelay::finish(request.id, false, core::Json{{"message", "No native screen is registered for \"" + request.plugin + "." + request.screen + "\"."}, {"code", "noHandler"}}.dump());
}

void Services::cancelScreen(std::uint64_t) {}

HaylenNativeWindow Services::getNativeWindow() {
    return {.handle = const_cast<void*>(sapp_win32_get_hwnd()), .display = nullptr};
}

} // namespace haylen::platform
