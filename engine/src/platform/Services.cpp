#include "platform/Services.hpp"

#include <stdexcept>
#include <utility>

#include "haylen/io/Package.hpp"
#include "platform/BridgeRelay.hpp"
#include "platform/ScreenRelay.hpp"

namespace haylen::platform {

port::Services& Services::installed = *new port::Services();

void Services::install(port::Services value) {
    if (value.platform == nullptr || value.displays == nullptr) {
        throw std::invalid_argument("The services of a platform need its platform and its displays. Register both in \"port::Services\".");
    }

    if (installed.platform != nullptr) {
        throw std::invalid_argument("The services of the platform were installed already. Install them once, before the first player of the process.");
    }

    installed = std::move(value);
}

const port::Services& Services::getInstalled() {
    if (installed.platform == nullptr) {
        throw std::logic_error("The runtime of the platform installed no services. Install them before the player starts.");
    }

    return installed;
}

std::string_view Services::getName() noexcept {
    return installed.platform->getName();
}

void Services::initialize() {
    getInstalled().platform->initialize();
}

void Services::shutdown() noexcept {
    installed.platform->shutdown();
}

void Services::reportError(const core::Json& report) {
    if (const auto& bridge = getInstalled().bridge) {
        bridge->reportError(report);
    }
}

std::vector<std::string> Services::getNativePlugins() {
    const auto& bridge = getInstalled().bridge;
    return bridge != nullptr ? bridge->getNativePlugins() : std::vector<std::string>();
}

std::shared_ptr<io::Package> Services::openBundledPackage() {
    return getInstalled().platform->openBundledPackage();
}

std::filesystem::path Services::getUserDataDirectory(std::string_view identifier) {
    return getInstalled().platform->getUserDataDirectory(identifier);
}

void Services::persistUserData() {
    getInstalled().platform->persistUserData();
}

std::string Services::getUserDataRefusal() {
    return getInstalled().platform->getUserDataRefusal();
}

SystemInfo Services::getSystemInfo() {
    return getInstalled().platform->getSystemInfo();
}

bool Services::hasPointerDevice() noexcept {
    return installed.platform->hasPointerDevice();
}

bool Services::hasKeyRepeats() noexcept {
    return installed.platform->hasKeyRepeats();
}

std::vector<Display> Services::getDisplays() {
    return getInstalled().displays->getDisplays();
}

std::string Services::getWindowDisplayId() {
    return getInstalled().displays->getWindowDisplayId();
}

bool Services::hasUnlistedDisplays() {
    return getInstalled().displays->hasUnlistedDisplays();
}

void Services::requestEveryScreen(std::function<void(SystemAnswer answer)> callback) {
    getInstalled().displays->requestEveryScreen(std::move(callback));
}

math::Rect Services::getWindowFrame() {
    return getInstalled().displays->getWindowFrame();
}

bool Services::hasDesktop() noexcept {
    return installed.desktop != nullptr;
}

// A window without a desktop fills its screen or the place that the system gives it, so it takes no style, background, place, passthrough or drag of its own.
void Services::setWindowStyle(const WindowStyle& value) {
    if (const auto& desktop = getInstalled().desktop) {
        desktop->setWindowStyle(value);
    }
}

void Services::setWindowBackground(const math::Color& value) {
    if (const auto& desktop = getInstalled().desktop) {
        desktop->setWindowBackground(value);
    }
}

// A window without a desktop to place it on, such as a window of visionOS in the space around the person, takes the size alone where the platform lets the app ask for one.
void Services::setWindowFrame(const math::Rect& value) {
    const port::Services& services = getInstalled();
    if (services.desktop != nullptr) {
        services.desktop->setWindowFrame(value);
        return;
    }

    if (services.windowSize != nullptr) {
        services.windowSize->requestSize(value.getSize());
    }
}

void Services::setMousePassthrough(Window::Passthrough mode, std::span<const math::Polygon::Outline> regions) {
    if (const auto& desktop = getInstalled().desktop) {
        desktop->setMousePassthrough(mode, regions);
    }
}

void Services::startWindowDrag() {
    if (const auto& desktop = getInstalled().desktop) {
        desktop->startWindowDrag();
    }
}

void Services::watchWindow() {
    if (const auto& desktop = getInstalled().desktop) {
        desktop->watchWindow();
    }
}

void Services::updateWindow() {
    if (const auto& desktop = getInstalled().desktop) {
        desktop->updateWindow();
    }
}

// Native libraries find no window of a desktop where the app has none.
HaylenNativeWindow Services::getNativeWindow() {
    const auto& desktop = getInstalled().desktop;
    return desktop != nullptr ? desktop->getNativeWindow() : HaylenNativeWindow{};
}

// A screen without the service is landscape and safe everywhere, with no status bar, gestures of the system, fold or cutouts.
math::Insets Services::getSafeAreaInsets() {
    const auto& screen = getInstalled().screen;
    return screen != nullptr ? screen->getSafeAreaInsets() : math::Insets{};
}

Orientation Services::getOrientation() {
    const auto& screen = getInstalled().screen;
    return screen != nullptr ? screen->getOrientation() : Orientation::Landscape;
}

void Services::lockOrientation(Orientation value) {
    if (const auto& screen = getInstalled().screen) {
        screen->lockOrientation(value);
    }
}

void Services::setStatusBar(Window::StatusBar value) {
    if (const auto& screen = getInstalled().screen) {
        screen->setStatusBar(value);
    }
}

float Services::getStatusBarHeight() {
    const auto& screen = getInstalled().screen;
    return screen != nullptr ? screen->getStatusBarHeight() : 0.0F;
}

void Services::setSystemGestures(Window::SystemGestures value) {
    if (const auto& screen = getInstalled().screen) {
        screen->setSystemGestures(value);
    }
}

std::optional<Fold> Services::getFold() {
    const auto& screen = getInstalled().screen;
    return screen != nullptr ? screen->getFold() : std::nullopt;
}

void Services::getCutouts(std::vector<math::Rect>& cutouts) {
    cutouts.clear();
    if (const auto& screen = getInstalled().screen) {
        screen->getCutouts(cutouts);
    }
}

// A platform without native parts of plugins has no handler for any call, which fails as it is made, so none is ever pending.
void Services::dispatch(std::uint64_t id, std::string_view method, std::string_view paramsJson, std::span<const std::vector<std::byte>> buffers, std::span<const varn::dataview::DataView> views) {
    if (const auto& bridge = getInstalled().bridge) {
        bridge->dispatch(id, method, paramsJson, buffers, views);
        return;
    }

    BridgeRelay::resolve(id, false, core::Json{{"message", "No native handler is registered for \"" + std::string(method) + "\"."}, {"code", "noHandler"}}.dump());
}

void Services::cancel(std::uint64_t id) {
    if (const auto& bridge = getInstalled().bridge) {
        bridge->cancel(id);
    }
}

void Services::openScreen(const ScreenRequest& request) {
    if (const auto& bridge = getInstalled().bridge) {
        bridge->openScreen(request);
        return;
    }

    ScreenRelay::finish(request.id, false, core::Json{{"message", "No native screen is registered for \"" + request.plugin + "." + request.screen + "\"."}, {"code", "noHandler"}}.dump());
}

void Services::cancelScreen(std::uint64_t id) {
    if (const auto& bridge = getInstalled().bridge) {
        bridge->cancelScreen(id);
    }
}

std::string Services::explainBlockedSocket(std::intptr_t socket) {
    const auto& network = getInstalled().network;
    return network != nullptr ? network->explainBlockedSocket(socket) : std::string();
}

std::string Services::getNetworkRequirement() {
    const auto& network = getInstalled().network;
    return network != nullptr ? network->getNetworkRequirement() : std::string();
}

// A platform that hands links to the running app itself never starts a process for a link, so no process forwards one.
bool Services::forwardLink(std::string_view identifier, std::string_view url) {
    const auto& links = getInstalled().links;
    return links != nullptr && links->forwardLink(identifier, url);
}

void Services::listenForLinks(std::string_view identifier) {
    if (const auto& links = getInstalled().links) {
        links->listenForLinks(identifier);
    }
}

// A platform that keeps every app running in the background lets every app play there.
bool Services::isBackgroundAudioDeclared() {
    const auto& backgroundAudio = getInstalled().backgroundAudio;
    return backgroundAudio == nullptr || backgroundAudio->isDeclared();
}

void Services::setBackgroundAudio(bool value) {
    if (const auto& backgroundAudio = getInstalled().backgroundAudio) {
        backgroundAudio->setEnabled(value);
    }
}

bool Services::isAccessibilityTreeWanted() {
    const auto& accessibility = getInstalled().accessibility;
    return accessibility != nullptr && accessibility->isTreeWanted();
}

void Services::updateAccessibility(std::span<const AccessibilityElement> elements, std::uint64_t focused) {
    if (const auto& accessibility = getInstalled().accessibility) {
        accessibility->update(elements, focused);
    }
}

void Services::announce(std::string_view text, bool interrupt) {
    if (const auto& accessibility = getInstalled().accessibility) {
        accessibility->announce(text, interrupt);
    }
}

std::unique_ptr<VideoDecoder> Services::openVideo(std::shared_ptr<io::PackageReader> file, std::string_view path) {
    const auto& video = getInstalled().video;
    return video != nullptr ? video->openVideo(std::move(file), path) : nullptr;
}

std::optional<SystemFont> Services::findSystemFont(char32_t codePoint) {
    const auto& fonts = getInstalled().fonts;
    return fonts != nullptr ? fonts->findSystemFont(codePoint) : std::nullopt;
}

// Only a face that `findSystemFont` found is ever read, which a platform without fonts of the system never finds.
std::vector<std::uint8_t> Services::readSystemFont(const SystemFont& font) {
    const auto& fonts = getInstalled().fonts;
    if (fonts == nullptr) {
        throw std::logic_error("The platform has no fonts of the system, so it reads no face such as \"" + font.name + "\".");
    }

    return fonts->readSystemFont(font);
}

std::shared_ptr<port::AudioOutput> Services::getAudioOutput() {
    return getInstalled().audio;
}

} // namespace haylen::platform
