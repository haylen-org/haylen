#include "platform/Services.hpp"

#include <cstdlib>
#include <stdexcept>
#include <string>

#include "haylen/io/Package.hpp"
#include "platform/linux/LinuxDesktop.hpp"
#include "platform/linux/LinuxGamepads.hpp"
#include "platform/linux/LinuxMethods.hpp"

namespace haylen::platform {

std::string_view Services::getName() noexcept {
    return "linux";
}

void Services::initialize() {
    LinuxGamepads::scan();
}

void Services::shutdown() noexcept {
    LinuxDesktop::release();
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

bool Services::hasDesktop() noexcept {
    return true;
}

void Services::setWindowStyle(const WindowStyle& value) {
    LinuxDesktop::setStyle(value);
}

math::Rect Services::getWindowFrame() {
    return LinuxDesktop::getFrame();
}

void Services::setWindowFrame(const math::Rect& value) {
    LinuxDesktop::setFrame(value);
}

std::vector<Monitor> Services::getMonitors() {
    return LinuxDesktop::getMonitors();
}

void Services::setMousePassthrough(Window::Passthrough mode, std::span<const math::Polygon::Outline> regions) {
    LinuxDesktop::setPassthrough(mode, regions);
}

void Services::startWindowDrag() {
    LinuxDesktop::startDrag();
}

void Services::watchWindow() {
    LinuxDesktop::watch();
}

void Services::updateWindow() {
    LinuxDesktop::update();
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
    static TextInput& input = *new TextInput();
    return input;
}

void Services::dispatch(std::uint64_t id, std::string_view method, std::string_view paramsJson) {
    LinuxMethods().dispatch(id, method, paramsJson);
}

// The desktop methods answer during the call, so no call of theirs is ever pending.
void Services::cancel(std::uint64_t) {}

} // namespace haylen::platform
