#include "platform/Services.hpp"

#include <cstdint>
#include <cstdlib>
#include <stdexcept>
#include <string>

#include "haylen/io/Package.hpp"
#include "platform/linux/LinuxGamepads.hpp"
#include "platform/linux/LinuxMethods.hpp"
#include "sokol_app.h"

// Xlib defines macros such as None and Bool, so it comes after the engine headers that use those names.
#include <X11/Xlib.h>
#include <X11/Xutil.h>

namespace haylen::platform {

std::string_view Services::getName() noexcept {
    return "linux";
}

void Services::initialize() {
    LinuxGamepads::scan();
}

void Services::shutdown() noexcept {
    LinuxGamepads::release();
}

void Services::reportError(const lua::Error&) {}

std::shared_ptr<io::Package> Services::openBundledPackage() {
    const std::filesystem::path directory = std::filesystem::read_symlink("/proc/self/exe").parent_path();
    if (std::filesystem::is_directory(directory / "app")) {
        return io::Package::open(directory / "app");
    }
    return io::Package::open(directory / "app.zip");
}

std::filesystem::path Services::getUserDataDirectory(std::string_view identifier) {
    if (const char* data = std::getenv("XDG_DATA_HOME"); data != nullptr && *data != '\0') {
        return std::filesystem::path(data) / std::string(identifier);
    }
    const char* home = std::getenv("HOME");
    if (home == nullptr) {
        throw std::runtime_error("The home folder is unknown, so there is no place for user data.");
    }
    return std::filesystem::path(home) / ".local" / "share" / std::string(identifier);
}

void Services::persistUserData() {}

// The window manager keeps a window whose smallest and largest sizes match at that size.
void Services::setWindowResizable(bool value) {
    auto* display = static_cast<Display*>(const_cast<void*>(sapp_x11_get_display()));
    const auto window = static_cast<::Window>(reinterpret_cast<std::uintptr_t>(sapp_x11_get_window()));
    XSizeHints hints{};
    if (!value) {
        hints.flags = PMinSize | PMaxSize;
        hints.min_width = hints.max_width = sapp_width();
        hints.min_height = hints.max_height = sapp_height();
    }
    XSetWMNormalHints(display, window, &hints);
    XFlush(display);
}

math::Insets Services::getSafeAreaInsets() {
    return {};
}

bool Services::hasPointerDevice() noexcept {
    return true;
}

void Services::pollGamepads(std::span<input::GamepadState> gamepads) {
    LinuxGamepads::poll(gamepads);
}

Orientation Services::getOrientation() {
    return Orientation::Landscape;
}

void Services::lockOrientation(Orientation) {}

// Linux types through the key and character events of X11, so its text input keeps the defaults.
TextInput& Services::getTextInput() {
    static TextInput input;
    return input;
}

void Services::dispatch(std::uint64_t id, std::string_view method, std::string_view paramsJson) {
    LinuxMethods().dispatch(id, method, paramsJson);
}

} // namespace haylen::platform
