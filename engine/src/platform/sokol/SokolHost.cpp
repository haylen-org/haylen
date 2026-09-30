#include "platform/sokol/SokolHost.hpp"

#include <stdexcept>
#include <string>
#include <utility>

#include "platform/Services.hpp"
#include "sokol_glue.h"
#include "sokol_log.h"

#if defined(__EMSCRIPTEN__)
#include <emscripten/emscripten.h>

#include "platform/web/BrowserAudioOutput.hpp"
#elif defined(__ANDROID__)
#include "platform/android/JavaBridge.hpp"
#endif

namespace haylen::platform {

math::Vec2 SokolHost::getFramebufferSize() const noexcept {
    return {sapp_widthf(), sapp_heightf()};
}

float SokolHost::getDpiScale() const noexcept {
    return sapp_dpi_scale();
}

bool SokolHost::isFullscreen() const noexcept {
    return sapp_is_fullscreen();
}

void SokolHost::setFullscreen(bool value) {
    if (sapp_is_fullscreen() == value) {
        return;
    }
    sapp_toggle_fullscreen();
    // Leaving fullscreen restores the default style of the window, which may have lost its decorations or its fixed size.
    if (!value) {
        applyStyle();
    }
}

bool SokolHost::isResizable() const noexcept {
    return style.resizable;
}

void SokolHost::setResizable(bool value) {
    style.resizable = value;
    applyStyle();
}

void SokolHost::setTitle(std::string_view value) {
    const std::string text(value);
    sapp_set_window_title(text.c_str());
#if defined(__EMSCRIPTEN__)
    // Sokol only writes the document title while it starts, so later titles reach the page here.
    emscripten_set_window_title(text.c_str());
#endif
}

void SokolHost::setCursor(Cursor value) {
    sapp_set_mouse_cursor(toSokolCursor(value));
}

void SokolHost::setCursorVisible(bool value) {
    sapp_show_mouse(value);
}

void SokolHost::setMouseLocked(bool value) {
    sapp_lock_mouse(value);
}

// The plain keyboard edits an empty native field of its own, placed in the corner of the screen so no browser scrolls the page to show it.
void SokolHost::setKeyboardVisible(bool value) {
    TextInput& input = Services::getTextInput();
    if (!value) {
        input.finish();
        return;
    }
    keyboard.reset();
    const math::Rect corner{0.0F, 0.0F, 1.0F, 1.0F};
    input.edit({.id = TextInput::kKeyboardField, .revision = ++keyboardRevision, .bounds = corner, .caret = corner, .options = {.capitalization = TextInput::Capitalization::None, .autocorrect = false}});
}

void SokolHost::setClipboard(std::string_view value) {
    sapp_set_clipboard_string(std::string(value).c_str());
}

std::string SokolHost::getClipboard() const {
    return sapp_get_clipboard_string();
}

void SokolHost::requestQuit() {
    sapp_request_quit();
}

Orientation SokolHost::getOrientation() const {
    return Services::getOrientation();
}

void SokolHost::lockOrientation(Orientation value) {
    Services::lockOrientation(value);
}

TextInput& SokolHost::getTextInput() noexcept {
    return Services::getTextInput();
}

bool SokolHost::hasPointerDevice() const noexcept {
    return Services::hasPointerDevice();
}

// The window composes premultiplied alpha with the desktop when it opened transparent, and while it is opaque the frames keep alpha 1.
bool SokolHost::canBeTransparent() const noexcept {
    return sapp_query_desc().composite_mode == SAPP_COMPOSITEMODE_PREMULTIPLIED;
}

bool SokolHost::isTransparent() const noexcept {
    return style.transparent;
}

void SokolHost::setTransparent(bool value) {
    if (value && !canBeTransparent()) {
        throw std::logic_error("The window opened opaque, so it cannot turn transparent. Set \"window.transparent\" in \"app.json\" to open a window that can.");
    }
    style.transparent = value;
    applyStyle();
}

bool SokolHost::isDecorated() const noexcept {
    return style.decorated;
}

void SokolHost::setDecorated(bool value) {
    style.decorated = value;
    applyStyle();
}

bool SokolHost::isAlwaysOnTop() const noexcept {
    return style.alwaysOnTop;
}

void SokolHost::setAlwaysOnTop(bool value) {
    style.alwaysOnTop = value;
    applyStyle();
}

bool SokolHost::isShownInTaskbar() const noexcept {
    return style.shownInTaskbar;
}

void SokolHost::setShowInTaskbar(bool value) {
    style.shownInTaskbar = value;
    applyStyle();
}

bool SokolHost::isFocusable() const noexcept {
    return focusable;
}

void SokolHost::setFocusable(bool value) {
    focusable = value;
    sapp_set_window_focusable(value);
}

math::Rect SokolHost::getFrame() const {
    return Services::getWindowFrame();
}

void SokolHost::setFrame(const math::Rect& value) {
    Services::setWindowFrame(value);
}

Window::Passthrough SokolHost::getMousePassthrough() const noexcept {
    return passthrough;
}

void SokolHost::setMousePassthrough(Passthrough mode, std::span<const math::Polygon::Outline> regions) {
    passthrough = mode;
    Services::setMousePassthrough(mode, regions);
}

void SokolHost::startDrag() {
    Services::startWindowDrag();
}

std::vector<Monitor> SokolHost::getMonitors() const {
    return Services::getMonitors();
}

void SokolHost::applyStyle() {
    if (!sapp_is_fullscreen()) {
        Services::setWindowStyle(style);
    }
}

std::string_view SokolHost::getPlatformName() const noexcept {
    return Services::getName();
}

graphics::DeviceSetup SokolHost::getGraphicsSetup() {
#if defined(NDEBUG)
    constexpr bool validation = false;
#else
    constexpr bool validation = true;
#endif
    return {.environment = sglue_environment(), .logger = {.func = slog_func}, .validation = validation};
}

audio::Mixer::Setup SokolHost::getAudioSetup() const {
#if defined(__EMSCRIPTEN__)
    return {.device = true, .backend = &audio::BrowserAudioOutput::kBackend};
#else
    return {.device = true};
#endif
}

graphics::FrameTarget SokolHost::getFrameTarget() {
    return {.swapchain = sglue_swapchain(), .transparent = style.transparent};
}

std::filesystem::path SokolHost::getUserDataDirectory(std::string_view identifier) {
    return Services::getUserDataDirectory(identifier);
}

void SokolHost::persistUserData() {
    Services::persistUserData();
}

math::Insets SokolHost::getSafeAreaInsets() const {
    return Services::getSafeAreaInsets();
}

void SokolHost::pollGamepads(std::span<input::GamepadState> gamepads) {
    Services::pollGamepads(gamepads);
}

void SokolHost::dispatchPlatformCall(std::uint64_t id, std::string_view method, std::string_view paramsJson, std::span<const std::vector<std::byte>> buffers) {
    Services::dispatch(id, method, paramsJson, buffers);
}

void SokolHost::cancelPlatformCall(std::uint64_t id) {
    Services::cancel(id);
}

math::Insets SokolHost::getReservedInsets() const {
    return getNativeViews().getReservedInsets();
}

bool SokolHost::isAppCovered() const {
    return getNativeViews().isAppCovered();
}

std::vector<std::string> SokolHost::getNativePlugins() const {
    return Services::getNativePlugins();
}

void SokolHost::reportError(const core::Json& report) {
    Services::reportError(report);
}

// The platform reads the system once per process, which is when an app of the process first starts.
SystemInfo SokolHost::getSystemInfo() const {
    static const SystemInfo& info = *new SystemInfo(Services::getSystemInfo());
    return info;
}

Theme SokolHost::getTheme() const {
    return getSystemState().getTheme();
}

Battery SokolHost::getBattery() const {
    return getSystemState().getBattery();
}

void SokolHost::openUrl(std::string_view url, std::function<void(bool opened)> callback) {
    Services::openUrl(std::string(url), std::move(callback));
}

void SokolHost::vibrate(float seconds) {
    Services::vibrate(seconds);
}

// Android is the only platform where the project of an app declares network access, through the permission `INTERNET` of its manifest.
std::string SokolHost::getNetworkRequirement() const {
#if defined(__ANDROID__)
    return JavaBridge::getNetworkRequirement();
#else
    return {};
#endif
}

void SokolHost::showDialog(std::uint64_t id, const DialogRequest& request, const std::filesystem::path& folder) {
    Services::showDialog(id, request, folder);
}

void SokolHost::cancelDialog(std::uint64_t id) {
    Services::cancelDialog(id);
}

void SokolHost::openScreen(const ScreenRequest& request) {
    Services::openScreen(request);
}

void SokolHost::cancelScreen(std::uint64_t id) {
    Services::cancelScreen(id);
}

NativeViews& SokolHost::getNativeViews() noexcept {
    static NativeViews& views = *new NativeViews();
    return views;
}

SystemState& SokolHost::getSystemState() noexcept {
    static SystemState& state = *new SystemState();
    return state;
}

sapp_mouse_cursor SokolHost::toSokolCursor(Cursor cursor) noexcept {
    switch (cursor) {
    case Cursor::Default:
        return SAPP_MOUSECURSOR_DEFAULT;
    case Cursor::Arrow:
        return SAPP_MOUSECURSOR_ARROW;
    case Cursor::IBeam:
        return SAPP_MOUSECURSOR_IBEAM;
    case Cursor::Crosshair:
        return SAPP_MOUSECURSOR_CROSSHAIR;
    case Cursor::PointingHand:
        return SAPP_MOUSECURSOR_POINTING_HAND;
    case Cursor::ResizeHorizontal:
        return SAPP_MOUSECURSOR_RESIZE_EW;
    case Cursor::ResizeVertical:
        return SAPP_MOUSECURSOR_RESIZE_NS;
    case Cursor::ResizeDiagonalDown:
        return SAPP_MOUSECURSOR_RESIZE_NWSE;
    case Cursor::ResizeDiagonalUp:
        return SAPP_MOUSECURSOR_RESIZE_NESW;
    case Cursor::ResizeAll:
        return SAPP_MOUSECURSOR_RESIZE_ALL;
    case Cursor::NotAllowed:
        return SAPP_MOUSECURSOR_NOT_ALLOWED;
    }
    return SAPP_MOUSECURSOR_DEFAULT;
}

} // namespace haylen::platform
