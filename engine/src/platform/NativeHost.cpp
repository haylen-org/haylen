#include "platform/NativeHost.hpp"

#include <stdexcept>
#include <string>
#include <utility>

#include "platform/CrashGuard.hpp"
#include "platform/Player.hpp"
#include "platform/Services.hpp"

namespace haylen::platform {

bool NativeHost::isResizable() const noexcept {
    return style.resizable;
}

void NativeHost::setResizable(bool value) {
    style.resizable = value;
    applyStyle();
}

void NativeHost::setKeyboardVisible(bool value) {
    textInput.setKeyboardVisible(value);
}

// A launched app quits back to the app that launched it, which takes over the same window.
void NativeHost::requestQuit() {
    if (!launched) {
        requestWindowClose();
    }
}

// An app that another app launched quits back to that app on every platform.
bool NativeHost::canQuit() const noexcept {
    return launched || allowsQuit();
}

Orientation NativeHost::getOrientation() const {
    return Services::getOrientation();
}

void NativeHost::lockOrientation(Orientation value) {
    Services::lockOrientation(value);
}

std::optional<Fold> NativeHost::getFold() const {
    return Services::getFold();
}

void NativeHost::getCutouts(std::vector<math::Rect>& cutouts) const {
    Services::getCutouts(cutouts);
}

Window::StatusBar NativeHost::getStatusBar() const noexcept {
    return statusBar;
}

void NativeHost::setStatusBar(StatusBar value) {
    statusBar = value;
    Services::setStatusBar(value);
}

Window::SystemGestures NativeHost::getSystemGestures() const noexcept {
    return systemGestures;
}

void NativeHost::setSystemGestures(SystemGestures value) {
    systemGestures = value;
    Services::setSystemGestures(value);
}

bool NativeHost::hasPointerDevice() const noexcept {
    return Services::hasPointerDevice();
}

bool NativeHost::isScreenKeptOn() const noexcept {
    return screenKeptOn;
}

void NativeHost::applyKeepScreenOn(bool value, std::function<void(SystemAnswer answer)> callback) {
    screenKeptOn = value;
    setScreenSaverSuspended(value, std::move(callback));
}

int NativeHost::getFrameRate() const noexcept {
    return frameRate;
}

void NativeHost::setFrameRate(int value) {
    requireFrameRate(value);
    frameRate = value;
    applyFrameRate(value);
}

float NativeHost::getMaxPixelDensity() const noexcept {
    return maxPixelDensity;
}

void NativeHost::setMaxPixelDensity(float value) {
    requireMaxPixelDensity(value);

    if (value == maxPixelDensity) {
        return;
    }

    maxPixelDensity = value;
    applyMaxPixelDensity(value);
}

bool NativeHost::isTransparent() const noexcept {
    return style.transparent;
}

void NativeHost::setTransparent(bool value) {
    if (value && !canBeTransparent()) {
        throw std::logic_error("The window opened opaque, so it cannot turn transparent. Set \"window.transparent\" in \"haylen.json\" to open a window that can.");
    }

    style.transparent = value;
    applyStyle();
}

bool NativeHost::isDecorated() const noexcept {
    return style.decorated;
}

void NativeHost::setDecorated(bool value) {
    style.decorated = value;
    applyStyle();
}

bool NativeHost::isAlwaysOnTop() const noexcept {
    return style.alwaysOnTop;
}

void NativeHost::setAlwaysOnTop(bool value) {
    style.alwaysOnTop = value;
    applyStyle();
}

bool NativeHost::isShownInTaskbar() const noexcept {
    return style.shownInTaskbar;
}

void NativeHost::setShowInTaskbar(bool value) {
    style.shownInTaskbar = value;
    applyStyle();
}

bool NativeHost::isFocusable() const noexcept {
    return focusable;
}

void NativeHost::setFocusable(bool value) {
    focusable = value;
    setWindowFocusable(value);
}

math::Rect NativeHost::getFrame() const {
    return Services::getWindowFrame();
}

void NativeHost::setFrame(const math::Rect& value) {
    Services::setWindowFrame(value);
}

Window::Passthrough NativeHost::getMousePassthrough() const noexcept {
    return passthrough;
}

void NativeHost::setMousePassthrough(Passthrough mode, std::span<const math::Polygon::Outline> regions) {
    passthrough = mode;
    Services::setMousePassthrough(mode, regions);
}

void NativeHost::startDrag() {
    Services::startWindowDrag();
}

std::vector<Display> NativeHost::getDisplays() const {
    return Services::getDisplays();
}

std::string NativeHost::getDisplayId() const {
    return Services::getWindowDisplayId();
}

bool NativeHost::hasUnlistedDisplays() const {
    return Services::hasUnlistedDisplays();
}

void NativeHost::requestEveryScreen(std::function<void(SystemAnswer answer)> callback) {
    Services::requestEveryScreen(std::move(callback));
}

FullscreenTarget NativeHost::resolveFullscreen(const Fullscreen& value) const {
    return FullscreenTarget::resolve(getDisplays(), getDisplayId(), value);
}

void NativeHost::applyFullscreenTarget(const Fullscreen& value, std::function<void(SystemAnswer answer)> callback) {
    const FullscreenTarget target = resolveFullscreen(value);
    if (target.display.id != getDisplayId() || target.mode) {
        callback({.state = SystemAnswer::State::Unsupported, .message = "The window of this platform stays on the display that shows it, in the mode that the system picks."});
        return;
    }

    setFullscreen(true, std::move(callback));
}

void NativeHost::openOutput(std::uint32_t number, std::string_view, const Output::Options&) {
    refuseOutput(number, "This platform shows the app on the display of its window alone.");
}

void NativeHost::closeOutput(std::uint32_t number) {
    refusedOutputs.erase(number);
}

NativeHost::OutputReport NativeHost::getOutputReport(std::uint32_t number) const {
    const auto found = refusedOutputs.find(number);
    return found != refusedOutputs.end() ? found->second : OutputReport{.ending = Output::Ending::Unsupported};
}

void NativeHost::setExternalDisplays(bool) {}

void NativeHost::refuseOutput(std::uint32_t number, std::string reason) {
    refusedOutputs[number] = {.ending = Output::Ending::Unsupported, .reason = std::move(reason)};
    Player::postEvent({.type = Event::Type::OutputChanged, .output = number});
}

void NativeHost::applyStyle() {
    if (!isFullscreen()) {
        Services::setWindowStyle(style);
    }
}

std::string_view NativeHost::getPlatformName() const noexcept {
    return Services::getName();
}

bool NativeHost::isFrameTransparent() const noexcept {
    return style.transparent;
}

std::filesystem::path NativeHost::getUserDataDirectory(std::string_view identifier) {
    return Services::getUserDataDirectory(identifier);
}

void NativeHost::persistUserData() {
    Services::persistUserData();
}

std::string NativeHost::getUserDataRefusal() {
    return Services::getUserDataRefusal();
}

math::Insets NativeHost::getSafeAreaInsets() const {
    return Services::getSafeAreaInsets();
}

float NativeHost::getStatusBarHeight() const {
    return Services::getStatusBarHeight();
}

void NativeHost::dispatchPlatformCall(std::uint64_t id, std::string_view method, std::string_view paramsJson, std::span<const std::vector<std::byte>> buffers, std::span<const varn::dataview::DataView> views) {
    Services::dispatch(id, method, paramsJson, buffers, views);
}

void NativeHost::cancelPlatformCall(std::uint64_t id) {
    Services::cancel(id);
}

math::Insets NativeHost::getReservedInsets() const {
    return getNativeViews().getReservedInsets();
}

bool NativeHost::isAppCovered() const {
    return getNativeViews().isAppCovered();
}

std::vector<std::string> NativeHost::getNativePlugins() const {
    return Services::getNativePlugins();
}

void NativeHost::reportError(const core::Json& report) {
    Services::reportError(report);
}

// The platform reads the system once per process, which is when an app of the process first starts.
SystemInfo NativeHost::getSystemInfo() const {
    static const SystemInfo& info = *new SystemInfo(Services::getSystemInfo());
    return info;
}

Theme NativeHost::getTheme() const {
    return getSystemState().getTheme();
}

Battery NativeHost::getBattery() const {
    return getSystemState().getBattery();
}

std::optional<bool> NativeHost::getPowerSaving() const {
    return getSystemState().getPowerSaving();
}

std::string NativeHost::getTimeZone() const {
    return getSystemState().getTimeZone();
}

std::vector<std::string> NativeHost::getLanguages() const {
    return getSystemState().getLanguages();
}

std::uint64_t NativeHost::getLanguagesRevision() const {
    return getSystemState().getLanguagesRevision();
}

AccessibilitySettings NativeHost::getAccessibilitySettings() const {
    return getSystemState().getAccessibility();
}

bool NativeHost::isBackgroundAudioDeclared() const {
    return Services::isBackgroundAudioDeclared();
}

void NativeHost::setBackgroundAudio(bool value) {
    Services::setBackgroundAudio(value);
}

bool NativeHost::isAccessibilityTreeWanted() const {
    return Services::isAccessibilityTreeWanted();
}

void NativeHost::updateAccessibility(std::span<const AccessibilityElement> elements, std::uint64_t focused) {
    Services::updateAccessibility(elements, focused);
}

void NativeHost::announce(std::string_view text, bool interrupt) {
    Services::announce(text, interrupt);
}

std::unique_ptr<VideoDecoder> NativeHost::openVideo(std::shared_ptr<io::PackageReader> file, std::string_view path) {
    return Services::openVideo(std::move(file), path);
}

std::optional<SystemFont> NativeHost::findSystemFont(char32_t codePoint) {
    return Services::findSystemFont(codePoint);
}

std::vector<std::uint8_t> NativeHost::readSystemFont(const SystemFont& font) {
    return Services::readSystemFont(font);
}

void NativeHost::guardCrashes(const std::filesystem::path& report) {
    CrashGuard::setReport(report);
}

void NativeHost::setCrashContext(std::string_view json) {
    CrashGuard::setContext(json);
}

Host::BlockedReason NativeHost::getBlockedReason() const {
    return &Services::explainBlockedSocket;
}

std::string NativeHost::getNetworkRequirement() const {
    return Services::getNetworkRequirement();
}

void NativeHost::openScreen(const ScreenRequest& request) {
    Services::openScreen(request);
}

void NativeHost::cancelScreen(std::uint64_t id) {
    Services::cancelScreen(id);
}

NativeViews& NativeHost::getNativeViews() noexcept {
    static NativeViews& views = *new NativeViews();
    return views;
}

SystemState& NativeHost::getSystemState() noexcept {
    static SystemState& state = *new SystemState();
    return state;
}

} // namespace haylen::platform
