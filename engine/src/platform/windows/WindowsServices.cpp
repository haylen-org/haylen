#include "platform/Services.hpp"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <knownfolders.h>
#include <shlobj.h>

#include <stdexcept>
#include <string>
#include <utility>

#include "content/FolderStoreFiles.hpp"
#include "content/ReleasePackage.hpp"
#include "haylen/content/Bootstrap.hpp"
#include "haylen/core/Json.hpp"
#include "haylen/io/Package.hpp"
#include "io/CompositePackage.hpp"
#include "platform/BridgeRelay.hpp"
#include "platform/NativeWindow.hpp"
#include "platform/ScreenRelay.hpp"
#include "platform/windows/WindowsAccessibility.hpp"
#include "platform/windows/WindowsDesktop.hpp"
#include "platform/windows/WindowsDialogs.hpp"
#include "platform/windows/WindowsMediaSession.hpp"
#include "platform/windows/WindowsSystem.hpp"
#include "platform/windows/WindowsVideoDecoder.hpp"

namespace haylen::platform {

std::string_view Services::getName() noexcept {
    return "windows";
}

void Services::initialize() {
    WindowsSystem::reportTheme();
    WindowsSystem::reportAccessibility();
    WindowsSystem::reportBattery();
}

void Services::shutdown() noexcept {
    WindowsAccessibility::release();
}

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
    // A release build plays only the release next to it, while a development build may also ship its package as a zip.
    const content::Bootstrap* bootstrap = content::Bootstrap::find();
    if (bootstrap != nullptr || std::filesystem::is_directory(directory / "app")) {
        // The shards of the packs that a store installs on their own, such as the expansions of a desktop game, land in `packs` next to the release.
        std::shared_ptr<io::Package> files = io::Package::openDirectory(directory / "app");
        if (std::filesystem::is_directory(directory / "packs")) {
            files = std::make_shared<io::CompositePackage>("app", std::vector<std::shared_ptr<const io::Package>>{files, io::Package::openDirectory(directory / "packs")});
        }
        return content::ReleasePackage::openBundled(std::move(files), bootstrap, [](std::string_view identifier) { return std::make_unique<content::FolderStoreFiles>(getContentStoreDirectory(identifier)); });
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

// Downloaded content is large and comes back with a download, so it stays on the machine instead of roaming with the profile.
std::filesystem::path Services::getContentStoreDirectory(std::string_view identifier) {
    PWSTR folder = nullptr;
    if (FAILED(SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &folder))) {
        CoTaskMemFree(folder);
        throw std::runtime_error("The local application data folder could not be found.");
    }
    std::filesystem::path path(folder);
    CoTaskMemFree(folder);
    return path / std::string(identifier) / "Content";
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

Orientation Services::getOrientation() {
    return Orientation::Landscape;
}

void Services::lockOrientation(Orientation) {}

// Windows desktops have no status bar over the window of an app.
void Services::setStatusBar(Window::StatusBar) {}

float Services::getStatusBarHeight() {
    return 0.0F;
}

// A Windows app opens its window at once, without a launch screen.
void Services::endSplashScreen(float) {}

// Windows offers desktop apps no API for the fold or the hinge of a device.
std::optional<Fold> Services::getFold() {
    return std::nullopt;
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

void Services::setNowPlaying(const std::optional<MediaSession::NowPlaying>& value) {
    WindowsMediaSession::update(value);
}

bool Services::isAccessibilityTreeWanted() {
    return WindowsAccessibility::isWanted();
}

void Services::updateAccessibility(std::span<const AccessibilityElement> elements, std::uint64_t focused) {
    WindowsAccessibility::update(elements, focused);
}

void Services::announce(std::string_view text, bool interrupt) {
    WindowsAccessibility::announce(text, interrupt);
}

std::unique_ptr<VideoDecoder> Services::openVideo(std::shared_ptr<io::PackageReader> file, std::string_view path) {
    return std::make_unique<WindowsVideoDecoder>(std::move(file), path);
}

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
    return {.handle = NativeWindow::getHandle(), .display = nullptr};
}

} // namespace haylen::platform
