#include "platform/Services.hpp"

#include <cstdlib>
#include <stdexcept>
#include <string>
#include <utility>

#include "haylen/core/Json.hpp"
#include "haylen/io/Package.hpp"
#include "platform/BridgeRelay.hpp"
#include "platform/DialogRelay.hpp"
#include "platform/ScreenRelay.hpp"
#include "platform/linux/LinuxDesktop.hpp"
#include "platform/linux/LinuxGamepads.hpp"
#include "platform/linux/LinuxSystem.hpp"
#include "sokol_app.h"

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

void Services::reportError(const core::Json&) {}

// The platform loads no native plugins of its own, and native libraries declare theirs through HaylenNativeApi.
std::vector<std::string> Services::getNativePlugins() {
    return {};
}

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

// Linux has no handler registry in the language of the platform, so native libraries and C++ plugins answer the methods of apps.
void Services::dispatch(std::uint64_t id, std::string_view method, std::string_view, std::span<const std::vector<std::byte>>) {
    BridgeRelay::resolve(id, false, core::Json{{"message", "No native handler is registered for " + std::string(method) + "."}, {"code", "noHandler"}}.dump());
}

// Every call fails while it is made, so no call is ever pending here.
void Services::cancel(std::uint64_t) {}

SystemInfo Services::getSystemInfo() {
    return LinuxSystem::getInfo();
}

void Services::openUrl(const std::string& url, std::function<void(bool opened)> callback) {
    LinuxSystem::openUrl(url, std::move(callback));
}

void Services::vibrate(float) {}

void Services::showDialog(std::uint64_t id, const DialogRequest&, const std::filesystem::path&) {
    DialogRelay::resolve(id, {.failure = DialogResult::Failure{.code = DialogResult::Code::Unsupported, .message = "Native dialogs are not implemented on Linux yet."}});
}

void Services::cancelDialog(std::uint64_t) {}

// Linux has no screen registry in the language of the platform, so native libraries open the screens of plugins, before the platform is asked.
void Services::openScreen(const ScreenRequest& request) {
    ScreenRelay::finish(request.id, false, core::Json{{"message", "No native screen is registered for " + request.plugin + "." + request.screen + "."}, {"code", "noHandler"}}.dump());
}

void Services::cancelScreen(std::uint64_t) {}

HaylenNativeWindow Services::getNativeWindow() {
    return {.handle = const_cast<void*>(sapp_x11_get_window()), .display = const_cast<void*>(sapp_x11_get_display())};
}

} // namespace haylen::platform
