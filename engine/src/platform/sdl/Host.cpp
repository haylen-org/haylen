#include "platform/sdl/Host.hpp"

#include <SDL3/SDL_clipboard.h>
#include <SDL3/SDL_events.h>
#include <SDL3/SDL_stdinc.h>

#include "platform/sdl/Runtime.hpp"

#if defined(__APPLE__)
#include <TargetConditionals.h>
#endif
#if defined(__APPLE__) && !TARGET_OS_OSX
#include "platform/apple/AppleGamepads.hpp"
#elif defined(__EMSCRIPTEN__)
#include "platform/web/WebPage.hpp"
#endif

namespace haylen::platform::sdl {

Host::~Host() {
    for (SDL_Cursor* cursor : cursors) {
        SDL_DestroyCursor(cursor);
    }
}

math::Vec2 Host::getFramebufferSize() const noexcept {
    return runtime.getWindow().getPixelSize();
}

// The scale is the framebuffer pixels per point, which SDL measures through the pixels per unit of the window and the scale of the display where window units are pixels, as on Windows and X11. A window that opened without high DPI keeps one framebuffer pixel per unit of the window.
float Host::getDpiScale() const noexcept {
    SDL_Window* window = runtime.getWindow().get();
    return runtime.isHighDpi() ? SDL_GetWindowDisplayScale(window) : SDL_GetWindowPixelDensity(window);
}

bool Host::isFullscreen() const noexcept {
    return (SDL_GetWindowFlags(runtime.getWindow().get()) & SDL_WINDOW_FULLSCREEN) != 0U;
}

void Host::setFullscreen(bool value) {
    if (isFullscreen() == value) {
        return;
    }
#if defined(__EMSCRIPTEN__)
    // The page puts the element that holds the canvas and its overlay in fullscreen, so the dialogs, the fields and the views of plugins stay over the app.
    WebPage::setFullscreen(value);
#else
    SDL_SetWindowFullscreen(runtime.getWindow().get(), value);
    // Leaving fullscreen restores the default style of the window, which may have lost its decorations or its fixed size.
    if (!value) {
        applyStyle();
    }
#endif
}

void Host::setTitle(std::string_view value) {
    SDL_SetWindowTitle(runtime.getWindow().get(), std::string(value).c_str());
}

void Host::setCursor(Cursor value) {
    if (value == Cursor::Default) {
        SDL_SetCursor(SDL_GetDefaultCursor());
        return;
    }
    SDL_Cursor*& cursor = cursors[static_cast<std::size_t>(value)];
    if (cursor == nullptr) {
        cursor = SDL_CreateSystemCursor(toSystemCursor(value));
    }
    SDL_SetCursor(cursor);
}

void Host::setCursorVisible(bool value) {
    if (value) {
        SDL_ShowCursor();
        return;
    }
    SDL_HideCursor();
}

void Host::setMouseLocked(bool value) {
    SDL_SetWindowRelativeMouseMode(runtime.getWindow().get(), value);
}

void Host::setClipboard(std::string_view value) {
    SDL_SetClipboardText(std::string(value).c_str());
}

std::string Host::getClipboard() const {
    char* text = SDL_GetClipboardText();
    std::string result = text != nullptr ? text : "";
    SDL_free(text);
    return result;
}

// The window composes premultiplied alpha with the desktop when it opened transparent, and while it is opaque the frames keep alpha 1.
bool Host::canBeTransparent() const noexcept {
    return runtime.isTransparent();
}

std::unique_ptr<gpu::Device> Host::createGpuDevice() {
    return runtime.createDevice();
}

void Host::closeWindow() {
    runtime.close();
}

// UIKit apps read their controllers through GameController, which reads the Siri Remote as a controller of its own while UIKit keeps its presses for the focus, and SDL reads them through GameController there too, so it would add nothing.
void Host::pollGamepads(std::span<input::GamepadState> gamepads) {
#if defined(__APPLE__) && !TARGET_OS_OSX
    AppleGamepads::poll(gamepads);
#else
    runtime.getGamepads().poll(gamepads);
#endif
}

void Host::rumbleGamepad(std::size_t index, float low, float high, float seconds) {
#if defined(__APPLE__) && !TARGET_OS_OSX
    AppleGamepads::rumble(index, low, high, seconds);
#else
    runtime.getGamepads().rumble(index, low, high, seconds);
#endif
}

void Host::requestWindowClose() {
    SDL_Event event{};
    event.type = SDL_EVENT_QUIT;
    SDL_PushEvent(&event);
}

void Host::setWindowFocusable(bool value) {
    SDL_SetWindowFocusable(runtime.getWindow().get(), value);
}

SDL_SystemCursor Host::toSystemCursor(Cursor cursor) noexcept {
    switch (cursor) {
    case Cursor::IBeam:
        return SDL_SYSTEM_CURSOR_TEXT;
    case Cursor::Crosshair:
        return SDL_SYSTEM_CURSOR_CROSSHAIR;
    case Cursor::PointingHand:
        return SDL_SYSTEM_CURSOR_POINTER;
    case Cursor::ResizeHorizontal:
        return SDL_SYSTEM_CURSOR_EW_RESIZE;
    case Cursor::ResizeVertical:
        return SDL_SYSTEM_CURSOR_NS_RESIZE;
    case Cursor::ResizeDiagonalDown:
        return SDL_SYSTEM_CURSOR_NWSE_RESIZE;
    case Cursor::ResizeDiagonalUp:
        return SDL_SYSTEM_CURSOR_NESW_RESIZE;
    case Cursor::ResizeAll:
        return SDL_SYSTEM_CURSOR_MOVE;
    case Cursor::NotAllowed:
        return SDL_SYSTEM_CURSOR_NOT_ALLOWED;
    default:
        return SDL_SYSTEM_CURSOR_DEFAULT;
    }
}

} // namespace haylen::platform::sdl
