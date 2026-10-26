#include "platform/Services.hpp"

#include <stdexcept>
#include <string>
#include <utility>

#include "TestPortWindow.hpp"
#include "haylen/core/Json.hpp"
#include "haylen/io/Package.hpp"
#include "platform/BridgeRelay.hpp"
#include "platform/DialogRelay.hpp"
#include "platform/ScreenRelay.hpp"

namespace haylen::platform {

std::string_view Services::getName() noexcept {
    return "test-port";
}

void Services::initialize() {}

void Services::shutdown() noexcept {}

void Services::reportError(const core::Json&) {}

std::vector<std::string> Services::getNativePlugins() {
    return {};
}

// The test port plays the package that its command line names, and ships none of its own.
std::shared_ptr<io::Package> Services::openBundledPackage() {
    throw std::runtime_error("The test port ships no app. Name the package of an app on the command line.");
}

std::filesystem::path Services::getUserDataDirectory(std::string_view identifier) {
    return std::filesystem::temp_directory_path() / "haylen-test-port" / std::string(identifier) / "data";
}

std::filesystem::path Services::getContentStoreDirectory(std::string_view identifier) {
    return std::filesystem::temp_directory_path() / "haylen-test-port" / std::string(identifier) / "content";
}

void Services::persistUserData() {}

// The test port has a screen, like a console, rather than a desktop of windows.
bool Services::hasDesktop() noexcept {
    return false;
}

void Services::setWindowStyle(const WindowStyle&) {}

math::Rect Services::getWindowFrame() {
    return {0.0F, 0.0F, TestPortWindow::kSize.x, TestPortWindow::kSize.y};
}

void Services::setWindowFrame(const math::Rect&) {}

std::vector<Monitor> Services::getMonitors() {
    return {Monitor{.name = "Test port", .bounds = getWindowFrame(), .workArea = getWindowFrame(), .scale = 1.0F, .primary = true}};
}

void Services::setMousePassthrough(Window::Passthrough, std::span<const math::Polygon::Outline>) {}

void Services::startWindowDrag() {}

void Services::watchWindow() {}

void Services::updateWindow() {}

math::Insets Services::getSafeAreaInsets() {
    return {};
}

bool Services::hasPointerDevice() noexcept {
    return false;
}

Orientation Services::getOrientation() {
    return Orientation::Landscape;
}

void Services::lockOrientation(Orientation) {}

void Services::setStatusBar(Window::StatusBar) {}

float Services::getStatusBarHeight() {
    return 0.0F;
}

void Services::endSplashScreen(float) {}

std::optional<Fold> Services::getFold() {
    return std::nullopt;
}

TextInput& Services::getTextInput() {
    static TextInput& input = *new TextInput();
    return input;
}

// The test port has no native handlers, so every call of an app fails as it is made.
void Services::dispatch(std::uint64_t id, std::string_view method, std::string_view, std::span<const std::vector<std::byte>>) {
    BridgeRelay::resolve(id, false, core::Json{{"message", "No native handler is registered for \"" + std::string(method) + "\"."}, {"code", "noHandler"}}.dump());
}

void Services::cancel(std::uint64_t) {}

SystemInfo Services::getSystemInfo() {
    return {.os = SystemInfo::Os::Port, .osVersion = "1.0", .deviceModel = "Test port", .deviceKind = SystemInfo::DeviceKind::Tv};
}

void Services::openUrl(const std::string&, std::function<void(bool opened)> callback) {
    callback(false);
}

void Services::vibrate(float) {}

void Services::setNowPlaying(const std::optional<MediaSession::NowPlaying>&) {}

bool Services::isAccessibilityTreeWanted() {
    return false;
}

void Services::updateAccessibility(std::span<const AccessibilityElement>, std::uint64_t) {}

void Services::announce(std::string_view, bool) {}

std::unique_ptr<VideoDecoder> Services::openVideo(std::shared_ptr<io::PackageReader>, std::string_view) {
    return nullptr;
}

void Services::showDialog(std::uint64_t id, const DialogRequest&, const std::filesystem::path&) {
    DialogRelay::resolve(id, {.failure = DialogResult::Failure{.code = DialogResult::Code::Unsupported, .message = "The test port shows no dialogs."}});
}

void Services::cancelDialog(std::uint64_t) {}

void Services::openScreen(const ScreenRequest& request) {
    ScreenRelay::finish(request.id, false, core::Json{{"message", "No native screen is registered for \"" + request.plugin + "." + request.screen + "\"."}, {"code", "noHandler"}}.dump());
}

void Services::cancelScreen(std::uint64_t) {}

HaylenNativeWindow Services::getNativeWindow() {
    return {.handle = nullptr, .display = nullptr};
}

} // namespace haylen::platform
