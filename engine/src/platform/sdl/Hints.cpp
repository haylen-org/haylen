#include "platform/sdl/Hints.hpp"

#include <SDL3/SDL_hints.h>
#include <SDL3/SDL_init.h>

#if defined(__APPLE__)
#include <TargetConditionals.h>
#elif defined(__EMSCRIPTEN__)
#include "platform/web/WebPage.hpp"
#endif

namespace haylen::platform::sdl {

// SDL names the app after its identifier, which Linux desktops match with the desktop entry of the app through the class of its window. The screen saver keeps its system settings, touches and the pen stay touches and the first click on a window in the background reaches the app, as with every native app of the platform.
void Hints::apply(const core::AppConfig& config) {
    SDL_SetAppMetadata(config.window.title.c_str(), nullptr, config.identifier.c_str());
    SDL_SetHint(SDL_HINT_VIDEO_ALLOW_SCREENSAVER, "1");
    SDL_SetHint(SDL_HINT_TOUCH_MOUSE_EVENTS, "0");
    SDL_SetHint(SDL_HINT_PEN_MOUSE_EVENTS, "0");
    SDL_SetHint(SDL_HINT_MOUSE_FOCUS_CLICKTHROUGH, "1");
#if !defined(__APPLE__) || TARGET_OS_OSX
    // Controllers reach the app also while its window has no focus, such as a window that never takes it above the taskbar.
    SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");
#endif
#if defined(__APPLE__) && TARGET_OS_TV
    // The presses of the Apple TV remote reach UIKit, which leaves the app on the Menu press that the app does not take.
    SDL_SetHint(SDL_HINT_APPLE_TV_CONTROLLER_UI_EVENTS, "1");
#elif defined(__EMSCRIPTEN__)
    SDL_SetHint(SDL_HINT_EMSCRIPTEN_CANVAS_SELECTOR, WebPage::getCanvasSelector().c_str());
#elif defined(__linux__)
    // Linux apps open their window on X11 first, also through XWayland in a Wayland session, since Wayland lets no app place its window, keep it above the others, leave the taskbar or let clicks through. A session without X11 opens it on Wayland, and the variable `SDL_VIDEO_DRIVER` picks either one.
    SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "x11,wayland");
#endif
}

} // namespace haylen::platform::sdl
