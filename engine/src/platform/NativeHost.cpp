#include "platform/NativeHost.hpp"

#include <stdexcept>
#include <string>
#include <utility>

#include "platform/CrashGuard.hpp"
#include "platform/Player.hpp"
#include "platform/Services.hpp"

#if defined(__EMSCRIPTEN__)
#include "platform/web/BrowserAudioOutput.hpp"
#elif defined(__ANDROID__)
#include "platform/android/AndroidActivity.hpp"
#include "platform/android/JavaBridge.hpp"
#elif defined(__APPLE__)
#include <TargetConditionals.h>
#endif

namespace haylen::platform {

bool NativeHost::isResizable() const noexcept {
    return style.resizable;
}

void NativeHost::setResizable(bool value) {
    style.resizable = value;
    applyStyle();
}

// The plain keyboard edits an empty native field of its own, placed in the corner of the screen so no browser scrolls the page to show it.
void NativeHost::setKeyboardVisible(bool value) {
    TextInput& input = getTextInput();
    if (!value) {
        input.finish();
        return;
    }
    keyboard.reset();
    const math::Rect corner{0.0F, 0.0F, 1.0F, 1.0F};
    input.edit({.id = TextInput::kKeyboardField, .revision = ++keyboardRevision, .bounds = corner, .caret = corner, .options = {.capitalization = TextInput::Capitalization::None}});
}

// A launched app quits back to the app that launched it, which takes over the same window.
void NativeHost::requestQuit() {
    if (!launched) {
        requestWindowClose();
    }
}

// An app that another app launched quits back to that app on every platform.
bool NativeHost::canQuit() const noexcept {
#if defined(__EMSCRIPTEN__) || (defined(__APPLE__) && !TARGET_OS_OSX)
    return launched;
#else
    return true;
#endif
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

void NativeHost::setKeepScreenOn(bool value, std::function<void(Permission::Answer answer)> callback) {
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

bool NativeHost::isTransparent() const noexcept {
    return style.transparent;
}

void NativeHost::setTransparent(bool value) {
    if (value && !canBeTransparent()) {
        throw std::logic_error("The window opened opaque, so it cannot turn transparent. Set \"window.transparent\" in \"app.json\" to open a window that can.");
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

FullscreenTarget NativeHost::resolveFullscreen(const Fullscreen& value) const {
    return FullscreenTarget::resolve(getDisplays(), getDisplayId(), value);
}

void NativeHost::enterFullscreen(const Fullscreen& value, std::function<void(Permission::Answer answer)> callback) {
    const FullscreenTarget target = resolveFullscreen(value);
    if (target.display.id != getDisplayId() || target.mode) {
        callback({.state = Permission::State::Unsupported, .message = "The window of this platform stays on the display that shows it, in the mode that the system picks."});
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

audio::Mixer::Setup NativeHost::getAudioSetup() const {
#if defined(__EMSCRIPTEN__)
    return {.device = true, .backend = &audio::BrowserAudioOutput::kBackend};
#else
    return {.device = true};
#endif
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

// The views of Android plugins reserve edges in window pixels.
math::Insets NativeHost::getReservedInsets() const {
#if defined(__ANDROID__)
    return AndroidActivity::toFramebuffer(getNativeViews().getReservedInsets());
#else
    return getNativeViews().getReservedInsets();
#endif
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

AccessibilitySettings NativeHost::getAccessibilitySettings() const {
    return getSystemState().getAccessibility();
}

void NativeHost::openUrl(std::string_view url, std::function<void(Permission::Answer answer)> callback) {
    Services::openUrl(std::string(url), std::move(callback));
}

void NativeHost::setNowPlaying(const std::optional<MediaSession::NowPlaying>& value) {
    Services::setNowPlaying(value);
}

Permission::Answer NativeHost::vibrate(float seconds) {
    return Services::vibrate(seconds);
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

void NativeHost::guardCrashes(const std::filesystem::path& report) {
    CrashGuard::install(report);
}

void NativeHost::setCrashContext(std::string_view json) {
    CrashGuard::setContext(json);
}

void NativeHost::checkPermission(Permission::Kind kind, std::function<void(Permission::Answer answer)> callback) {
    Services::checkPermission(kind, std::move(callback));
}

void NativeHost::requestPermission(Permission::Kind kind, std::function<void(Permission::Answer answer)> callback) {
    Services::requestPermission(kind, std::move(callback));
}

Host::BlockedReason NativeHost::getBlockedReason() const {
    return &Services::explainBlockedSocket;
}

// Android is the only platform where the project of an app declares network access, through the permission `INTERNET` of its manifest.
std::string NativeHost::getNetworkRequirement() const {
#if defined(__ANDROID__)
    return JavaBridge::getNetworkRequirement();
#else
    return {};
#endif
}

void NativeHost::showDialog(std::uint64_t id, const DialogRequest& request, const std::filesystem::path& folder) {
    Services::showDialog(id, request, folder);
}

void NativeHost::cancelDialog(std::uint64_t id) {
    Services::cancelDialog(id);
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
