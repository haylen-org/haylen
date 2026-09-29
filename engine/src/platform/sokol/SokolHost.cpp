#include "platform/sokol/SokolHost.hpp"

#include <string>

#include "platform/Services.hpp"
#include "sokol_glue.h"
#include "sokol_log.h"

#if defined(__EMSCRIPTEN__)
#include <emscripten/emscripten.h>
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
    if (sapp_is_fullscreen() != value) {
        sapp_toggle_fullscreen();
        // Leaving fullscreen restores the default frame of the window, which lets the player resize it again.
        Services::setWindowResizable(resizable);
    }
}

bool SokolHost::isResizable() const noexcept {
    return resizable;
}

void SokolHost::setResizable(bool value) {
    resizable = value;
    Services::setWindowResizable(value);
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
    return {.device = true};
}

graphics::FrameTarget SokolHost::getFrameTarget() {
    return {.swapchain = sglue_swapchain()};
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

void SokolHost::dispatchPlatformCall(std::uint64_t id, std::string_view method, std::string_view paramsJson) {
    Services::dispatch(id, method, paramsJson);
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
