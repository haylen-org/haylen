#include "platform/windows/WindowsServices.hpp"

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
#include "haylen/io/Package.hpp"
#include "io/CompositePackage.hpp"
#include "platform/NativeWindow.hpp"
#include "platform/SystemFontReader.hpp"
#include "platform/windows/WindowsAccessibility.hpp"
#include "platform/windows/WindowsDesktop.hpp"
#include "platform/windows/WindowsFonts.hpp"
#include "platform/windows/WindowsLinks.hpp"
#include "platform/windows/WindowsMediaSession.hpp"
#include "platform/windows/WindowsMemory.hpp"
#include "platform/windows/WindowsNetwork.hpp"
#include "platform/windows/WindowsSystem.hpp"
#include "platform/windows/WindowsVideoDecoder.hpp"

namespace haylen::platform {

port::Services WindowsServices::create() {
    const auto services = std::make_shared<WindowsServices>();
    return {.platform = services, .displays = services, .desktop = services, .links = services, .media = services, .accessibility = services, .video = services, .fonts = services};
}

std::string_view WindowsServices::getName() const noexcept {
    return "windows";
}

void WindowsServices::initialize() {
    WindowsSystem::reportTheme();
    WindowsSystem::reportLanguages();
    WindowsSystem::reportAccessibility();
    WindowsSystem::reportPowerStatus();
    WindowsSystem::reportTimeZone();
    WindowsMemory::observe();
}

void WindowsServices::shutdown() noexcept {
    WindowsAccessibility::release();
}

std::shared_ptr<io::Package> WindowsServices::openBundledPackage() {
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

std::filesystem::path WindowsServices::getUserDataDirectory(std::string_view identifier) {
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
std::filesystem::path WindowsServices::getContentStoreDirectory(std::string_view identifier) {
    PWSTR folder = nullptr;
    if (FAILED(SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &folder))) {
        CoTaskMemFree(folder);
        throw std::runtime_error("The local application data folder could not be found.");
    }

    std::filesystem::path path(folder);
    CoTaskMemFree(folder);
    return path / std::string(identifier) / "Content";
}

void WindowsServices::persistUserData() {}

void WindowsServices::setWindowStyle(const WindowStyle& value) {
    WindowsDesktop::setStyle(value);
}

// Windows shows the frames that the app drew, so the window needs no color of its own.
void WindowsServices::setWindowBackground(const math::Color&) {}

math::Rect WindowsServices::getWindowFrame() {
    return WindowsDesktop::getFrame();
}

void WindowsServices::setWindowFrame(const math::Rect& value) {
    WindowsDesktop::setFrame(value);
}

std::vector<Display> WindowsServices::getDisplays() {
    return WindowsDesktop::getDisplays();
}

std::string WindowsServices::getWindowDisplayId() {
    return WindowsDesktop::getWindowDisplayId();
}

// Every display of the device is in the list, so a request for every screen has nothing to ask for.
bool WindowsServices::hasUnlistedDisplays() {
    return false;
}

void WindowsServices::requestEveryScreen(std::function<void(Permission::Answer answer)> callback) {
    callback({.state = Permission::State::Granted});
}

void WindowsServices::setMousePassthrough(Window::Passthrough mode, std::span<const math::Polygon::Outline> regions) {
    WindowsDesktop::setPassthrough(mode, regions);
}

void WindowsServices::startWindowDrag() {
    WindowsDesktop::startDrag();
}

void WindowsServices::watchWindow() {
    WindowsDesktop::watch();
    WindowsNetwork::observe();
}

void WindowsServices::updateWindow() {
    WindowsDesktop::update();
}

bool WindowsServices::hasPointerDevice() const noexcept {
    return true;
}

SystemInfo WindowsServices::getSystemInfo() {
    return WindowsSystem::getInfo();
}

bool WindowsServices::forwardLink(std::string_view identifier, std::string_view url) {
    return WindowsLinks::forward(identifier, url);
}

void WindowsServices::listenForLinks(std::string_view identifier) {
    WindowsLinks::listen(identifier);
}

// A minimized window keeps running its frames, so its sound plays on.
void WindowsServices::setBackgroundAudio(bool) {}

void WindowsServices::setNowPlaying(const std::optional<MediaSession::NowPlaying>& value) {
    WindowsMediaSession::update(value);
}

bool WindowsServices::isTreeWanted() {
    return WindowsAccessibility::isWanted();
}

void WindowsServices::update(std::span<const AccessibilityElement> elements, std::uint64_t focused) {
    WindowsAccessibility::update(elements, focused);
}

void WindowsServices::announce(std::string_view text, bool interrupt) {
    WindowsAccessibility::announce(text, interrupt);
}

std::optional<SystemFont> WindowsServices::findSystemFont(char32_t codePoint) {
    return WindowsFonts::find(codePoint);
}

std::vector<std::uint8_t> WindowsServices::readSystemFont(const SystemFont& font) {
    return SystemFontReader::read(font.file);
}

std::unique_ptr<VideoDecoder> WindowsServices::openVideo(std::shared_ptr<io::PackageReader> file, std::string_view path) {
    return std::make_unique<WindowsVideoDecoder>(std::move(file), path);
}

HaylenNativeWindow WindowsServices::getNativeWindow() {
    return {.handle = NativeWindow::getHandle(), .display = nullptr};
}

} // namespace haylen::platform
