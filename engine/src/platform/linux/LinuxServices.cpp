#include "platform/linux/LinuxServices.hpp"

#include <cstdlib>
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
#include "platform/linux/LinuxAccessibility.hpp"
#include "platform/linux/LinuxDesktop.hpp"
#include "platform/linux/LinuxFonts.hpp"
#include "platform/linux/LinuxGlib.hpp"
#include "platform/linux/LinuxLinks.hpp"
#include "platform/linux/LinuxMonitors.hpp"
#include "platform/linux/LinuxSettings.hpp"
#include "platform/linux/LinuxSystem.hpp"

namespace haylen::platform {

port::Services LinuxServices::create() {
    const auto services = std::make_shared<LinuxServices>();
    return {.platform = services, .displays = services, .desktop = services, .links = services, .accessibility = services, .fonts = services};
}

std::string_view LinuxServices::getName() const noexcept {
    return "linux";
}

void LinuxServices::initialize() {
    LinuxSystem::reportLanguages();
    LinuxSystem::watchBattery();
    LinuxSettings::observe();
    LinuxMonitors::observe();
    LinuxAccessibility::start();
}

void LinuxServices::shutdown() noexcept {
    LinuxDesktop::release();
    LinuxFonts::release();
}

std::shared_ptr<io::Package> LinuxServices::openBundledPackage() {
    const std::filesystem::path directory = std::filesystem::read_symlink("/proc/self/exe").parent_path();
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

std::filesystem::path LinuxServices::getUserDataDirectory(std::string_view identifier) {
    if (const char* data = std::getenv("XDG_DATA_HOME"); data != nullptr && *data != '\0') {
        return std::filesystem::path(data) / std::string(identifier);
    }

    const char* home = std::getenv("HOME");
    if (home == nullptr) {
        throw std::runtime_error("The home folder is unknown, so there is no place for user data.");
    }

    return std::filesystem::path(home) / ".local" / "share" / std::string(identifier);
}

// Downloaded content comes back with a download, so it lives among the caches of the user.
std::filesystem::path LinuxServices::getContentStoreDirectory(std::string_view identifier) {
    if (const char* cache = std::getenv("XDG_CACHE_HOME"); cache != nullptr && *cache != '\0') {
        return std::filesystem::path(cache) / std::string(identifier) / "content";
    }

    const char* home = std::getenv("HOME");
    if (home == nullptr) {
        throw std::runtime_error("The home folder is unknown, so there is no place for downloaded content.");
    }

    return std::filesystem::path(home) / ".cache" / std::string(identifier) / "content";
}

void LinuxServices::persistUserData() {}

// Nothing keeps the user data of the apps from them on this platform.
std::string LinuxServices::getUserDataRefusal() {
    return {};
}

SystemInfo LinuxServices::getSystemInfo() {
    return LinuxSystem::getInfo();
}

bool LinuxServices::hasPointerDevice() const noexcept {
    return true;
}

void LinuxServices::setWindowStyle(const WindowStyle& value) {
    LinuxDesktop::setStyle(value);
}

void LinuxServices::setWindowBackground(const math::Color& value) {
    LinuxDesktop::setBackground(value);
}

math::Rect LinuxServices::getWindowFrame() {
    return LinuxDesktop::getFrame();
}

void LinuxServices::setWindowFrame(const math::Rect& value) {
    LinuxDesktop::setFrame(value);
}

std::vector<Display> LinuxServices::getDisplays() {
    return LinuxDesktop::getDisplays();
}

std::string LinuxServices::getWindowDisplayId() {
    return LinuxDesktop::getWindowDisplayId();
}

// Every display of the device is in the list, so a request for every screen has nothing to ask for.
bool LinuxServices::hasUnlistedDisplays() {
    return false;
}

void LinuxServices::requestEveryScreen(std::function<void(SystemAnswer answer)> callback) {
    callback({.state = SystemAnswer::State::Granted});
}

void LinuxServices::setMousePassthrough(Window::Passthrough mode, std::span<const math::Polygon::Outline> regions) {
    LinuxDesktop::setPassthrough(mode, regions);
}

void LinuxServices::startWindowDrag() {
    LinuxDesktop::startDrag();
}

void LinuxServices::watchWindow() {
    LinuxDesktop::watch();
}

void LinuxServices::updateWindow() {
    LinuxDesktop::update();
    LinuxGlib::iterate();
}

HaylenNativeWindow LinuxServices::getNativeWindow() {
    return {.handle = NativeWindow::getHandle(), .display = NativeWindow::getDisplay()};
}

bool LinuxServices::forwardLink(std::string_view identifier, std::string_view url) {
    return LinuxLinks::forward(identifier, url);
}

void LinuxServices::listenForLinks(std::string_view identifier) {
    LinuxLinks::listen(identifier);
}

bool LinuxServices::isTreeWanted() {
    return LinuxAccessibility::isWanted();
}

void LinuxServices::update(std::span<const AccessibilityElement> elements, std::uint64_t focused) {
    LinuxAccessibility::update(elements, focused);
}

void LinuxServices::announce(std::string_view text, bool interrupt) {
    LinuxAccessibility::announce(text, interrupt);
}

std::optional<SystemFont> LinuxServices::findSystemFont(char32_t codePoint) {
    return LinuxFonts::find(codePoint);
}

std::vector<std::uint8_t> LinuxServices::readSystemFont(const SystemFont& font) {
    return SystemFontReader::read(font.file);
}

} // namespace haylen::platform
