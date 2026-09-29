#include "platform/Services.hpp"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <knownfolders.h>
#include <shlobj.h>
#include <xinput.h>

#include <stdexcept>
#include <string>

#include "haylen/io/Package.hpp"
#include "platform/windows/WindowsMethods.hpp"
#include "platform/windows/WindowsTextInput.hpp"
#include "sokol_app.h"

namespace haylen::platform {

std::string_view Services::getName() noexcept {
    return "windows";
}

void Services::initialize() {}

void Services::shutdown() noexcept {}

void Services::reportError(const lua::Error&) {}

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

void Services::setWindowResizable(bool value) {
    auto* window = static_cast<HWND>(const_cast<void*>(sapp_win32_get_hwnd()));
    constexpr LONG_PTR kFrame = WS_SIZEBOX | WS_MAXIMIZEBOX;
    const LONG_PTR style = GetWindowLongPtrW(window, GWL_STYLE);
    SetWindowLongPtrW(window, GWL_STYLE, value ? (style | kFrame) : (style & ~kFrame));
    SetWindowPos(window, nullptr, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
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
    static WindowsTextInput input;
    return input;
}

void Services::dispatch(std::uint64_t id, std::string_view method, std::string_view paramsJson) {
    WindowsMethods().dispatch(id, method, paramsJson);
}

} // namespace haylen::platform
