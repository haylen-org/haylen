#include "platform/sdl/Events.hpp"

#include <cstring>
#include <utility>

#include <SDL3/SDL_stdinc.h>
#include <SDL3/SDL_video.h>

#include "platform/sdl/Keys.hpp"

namespace haylen::platform::sdl {

void Events::translate(const SDL_Event& source, const Context& context, std::vector<platform::Event>& events) {
    switch (source.type) {
    case SDL_EVENT_KEY_DOWN:
    case SDL_EVENT_KEY_UP:
        if (const std::optional<platform::Event> key = translateKey(source.key)) {
            events.push_back(*key);
        }

        return;
    case SDL_EVENT_TEXT_INPUT:
        appendText(source.text.text, context, events);
        return;
    case SDL_EVENT_MOUSE_BUTTON_DOWN:
    case SDL_EVENT_MOUSE_BUTTON_UP:
    case SDL_EVENT_MOUSE_MOTION:
    case SDL_EVENT_MOUSE_WHEEL:
        if (const std::optional<platform::Event> pointer = translatePointer(source, context.density, context.modifiers)) {
            events.push_back(*pointer);
        }

        return;
    case SDL_EVENT_FINGER_DOWN:
    case SDL_EVENT_FINGER_MOTION:
    case SDL_EVENT_FINGER_UP:
    case SDL_EVENT_FINGER_CANCELED:
        if (!kRemote) {
            appendTouch(source.tfinger, context, events);
        }

        return;
    case SDL_EVENT_QUIT:
        events.push_back({.type = platform::Event::Type::QuitRequested});
        return;
    default:
        break;
    }

    // The services of each platform report the displays too, and the engine publishes only what changed, so a report of both reaches the app once.
    if (source.type >= SDL_EVENT_DISPLAY_FIRST && source.type <= SDL_EVENT_DISPLAY_LAST) {
        events.push_back({.type = platform::Event::Type::DisplaysChanged});
        return;
    }

    if (source.type >= SDL_EVENT_WINDOW_FIRST && source.type <= SDL_EVENT_WINDOW_LAST) {
        if (const std::optional<platform::Event> window = translateWindow(source, context)) {
            events.push_back(*window);
        }

        return;
    }

    if (const std::optional<platform::Event> app = translateApp(source)) {
        events.push_back(*app);
    }
}

// Buttons beyond the three the engine names, such as the back and forward buttons of a mouse, reach the app as nothing. The wheels of SDL scroll right with positive steps, while the engine scrolls left with them.
std::optional<platform::Event> Events::translatePointer(const SDL_Event& source, float scale, SDL_Keymod modifiers) noexcept {
    const input::KeyModifiers held = toModifiers(modifiers);
    switch (source.type) {
    case SDL_EVENT_MOUSE_BUTTON_DOWN:
    case SDL_EVENT_MOUSE_BUTTON_UP: {
        const std::optional<input::MouseButton> button = toMouseButton(source.button.button);
        if (!button) {
            return std::nullopt;
        }

        const platform::Event::Type type = source.type == SDL_EVENT_MOUSE_BUTTON_DOWN ? platform::Event::Type::MouseDown : platform::Event::Type::MouseUp;
        return platform::Event{.type = type, .modifiers = held, .mouseButton = *button, .position = {source.button.x * scale, source.button.y * scale}};
    }
    case SDL_EVENT_MOUSE_MOTION:
        return platform::Event{.type = platform::Event::Type::MouseMove, .modifiers = held, .position = {source.motion.x * scale, source.motion.y * scale}, .delta = {source.motion.xrel * scale, source.motion.yrel * scale}};
    case SDL_EVENT_MOUSE_WHEEL:
        return platform::Event{.type = platform::Event::Type::MouseScroll, .modifiers = held, .position = {source.wheel.mouse_x * scale, source.wheel.mouse_y * scale}, .scroll = {-source.wheel.x, source.wheel.y}};
    default:
        return std::nullopt;
    }
}

SDL_WindowID Events::getWindowId(const SDL_Event& source) noexcept {
    if (source.type >= SDL_EVENT_WINDOW_FIRST && source.type <= SDL_EVENT_WINDOW_LAST) {
        return source.window.windowID;
    }

    switch (source.type) {
    case SDL_EVENT_KEY_DOWN:
    case SDL_EVENT_KEY_UP:
        return source.key.windowID;
    case SDL_EVENT_TEXT_INPUT:
        return source.text.windowID;
    case SDL_EVENT_TEXT_EDITING:
        return source.edit.windowID;
    case SDL_EVENT_MOUSE_BUTTON_DOWN:
    case SDL_EVENT_MOUSE_BUTTON_UP:
        return source.button.windowID;
    case SDL_EVENT_MOUSE_MOTION:
        return source.motion.windowID;
    case SDL_EVENT_MOUSE_WHEEL:
        return source.wheel.windowID;
    case SDL_EVENT_FINGER_DOWN:
    case SDL_EVENT_FINGER_MOTION:
    case SDL_EVENT_FINGER_UP:
    case SDL_EVENT_FINGER_CANCELED:
        return source.tfinger.windowID;
    default:
        return 0;
    }
}

bool Events::isOfClosedWindow(const SDL_Event& source) noexcept {
    const SDL_WindowID window = getWindowId(source);
    return window != 0 && SDL_GetWindowFromID(window) == nullptr;
}

// SDL turns the Menu press of the remote into escape.
bool Events::isPlatformBack(const SDL_Event& source) noexcept {
    return kRemote && (source.type == SDL_EVENT_KEY_DOWN || source.type == SDL_EVENT_KEY_UP) && source.key.scancode == SDL_SCANCODE_ESCAPE;
}

input::KeyModifiers Events::toModifiers(SDL_Keymod modifiers) noexcept {
    return {.shift = (modifiers & SDL_KMOD_SHIFT) != 0U, .control = (modifiers & SDL_KMOD_CTRL) != 0U, .alt = (modifiers & SDL_KMOD_ALT) != 0U, .super = (modifiers & SDL_KMOD_GUI) != 0U};
}

std::optional<input::MouseButton> Events::toMouseButton(std::uint8_t button) noexcept {
    switch (button) {
    case SDL_BUTTON_LEFT:
        return input::MouseButton::Left;
    case SDL_BUTTON_RIGHT:
        return input::MouseButton::Right;
    case SDL_BUTTON_MIDDLE:
        return input::MouseButton::Middle;
    default:
        return std::nullopt;
    }
}

std::optional<platform::Event> Events::translateKey(const SDL_KeyboardEvent& source) noexcept {
    const platform::Event::Type type = source.down ? platform::Event::Type::KeyDown : platform::Event::Type::KeyUp;
    const input::Key key = Keys::fromScancode(source.scancode);
    if (key == input::Key::Unknown) {
        return std::nullopt;
    }

    if (source.scancode < SDL_SCANCODE_COUNT) {
        held.set(static_cast<std::size_t>(source.scancode), source.down);
    }

    return platform::Event{.type = type, .key = key, .repeat = source.repeat, .modifiers = toModifiers(source.mod)};
}

// A desktop window that comes back from the taskbar resumes the app, while a window that only leaves its maximized size does not.
std::optional<platform::Event> Events::translateWindow(const SDL_Event& source, const Context& context) noexcept {
    switch (source.type) {
    case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
    case SDL_EVENT_WINDOW_DISPLAY_SCALE_CHANGED:
    case SDL_EVENT_WINDOW_METAL_VIEW_RESIZED:
        return platform::Event{.type = platform::Event::Type::Resized};
    case SDL_EVENT_WINDOW_FOCUS_GAINED:
    case SDL_EVENT_WINDOW_FOCUS_LOST:
        return kAppStates ? std::nullopt : translateFocus(source.type == SDL_EVENT_WINDOW_FOCUS_GAINED, context);
    case SDL_EVENT_WINDOW_DISPLAY_CHANGED:
        return platform::Event{.type = platform::Event::Type::WindowMoved};
    case SDL_EVENT_WINDOW_MOUSE_ENTER:
        return platform::Event{.type = platform::Event::Type::MouseEnter};
    case SDL_EVENT_WINDOW_MOUSE_LEAVE:
        return platform::Event{.type = platform::Event::Type::MouseLeave};
    case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
        return platform::Event{.type = platform::Event::Type::QuitRequested};
    case SDL_EVENT_WINDOW_MINIMIZED:
        if (kAppStates) {
            return std::nullopt;
        }

        minimized = true;
        return platform::Event{.type = platform::Event::Type::Suspended};
    case SDL_EVENT_WINDOW_RESTORED:
        if (kAppStates || !minimized) {
            return std::nullopt;
        }

        minimized = false;
        return platform::Event{.type = platform::Event::Type::Resumed};
    default:
        return std::nullopt;
    }
}

// The windows of a desktop app share its focus, so the keyboard that goes from the window of the app to the window of an output or back keeps the app focused, and the app loses the focus once the keyboard leaves every window of the app, whichever window it leaves from.
std::optional<platform::Event> Events::translateFocus(bool gained, const Context& context) noexcept {
    if (!gained && context.appHoldsKeyboard) {
        return std::nullopt;
    }

    if (focused == gained) {
        return std::nullopt;
    }

    focused = gained;
    return platform::Event{.type = gained ? platform::Event::Type::FocusGained : platform::Event::Type::FocusLost};
}

// The scene of a UIKit app that resigns active, under the app switcher, Control Center or a call, loses the focus while its frames go on, and the app suspends only once the scene enters the background. SDL reports each change for the scene and again for the application, and the app hears it once.
std::optional<platform::Event> Events::translateApp(const SDL_Event& source) noexcept {
    platform::Event::Type type = platform::Event::Type::FocusGained;
    switch (source.type) {
    case SDL_EVENT_WILL_ENTER_BACKGROUND:
        type = platform::Event::Type::FocusLost;
        break;
    case SDL_EVENT_DID_ENTER_BACKGROUND:
        type = platform::Event::Type::Suspended;
        break;
    case SDL_EVENT_WILL_ENTER_FOREGROUND:
        type = platform::Event::Type::Resumed;
        break;
    case SDL_EVENT_DID_ENTER_FOREGROUND:
        type = platform::Event::Type::FocusGained;
        break;
    default:
        return std::nullopt;
    }

    if (type == appState) {
        return std::nullopt;
    }

    appState = type;
    return platform::Event{.type = type};
}

void Events::appendText(const char* text, const Context& context, std::vector<platform::Event>& events) {
    const input::KeyModifiers held = toModifiers(context.modifiers);
    std::size_t left = std::strlen(text);
    while (left > 0) {
        const Uint32 character = SDL_StepUTF8(&text, &left);
        if (character != 0 && character != SDL_INVALID_UNICODE_CODEPOINT) {
            events.push_back({.type = platform::Event::Type::Character, .modifiers = held, .character = static_cast<char32_t>(character)});
        }
    }
}

void Events::appendTouch(const SDL_TouchFingerEvent& source, const Context& context, std::vector<platform::Event>& events) {
    Touches::Phase phase = Touches::Phase::Moved;
    if (source.type == SDL_EVENT_FINGER_DOWN) {
        phase = Touches::Phase::Began;
    } else if (source.type == SDL_EVENT_FINGER_UP) {
        phase = Touches::Phase::Ended;
    } else if (source.type == SDL_EVENT_FINGER_CANCELED) {
        phase = Touches::Phase::Cancelled;
    }

    const math::Vec2 position{source.x * context.framebuffer.x, source.y * context.framebuffer.y};
    if (std::optional<platform::Event> touch = touches.change(static_cast<std::uint64_t>(source.fingerID), phase, position, toModifiers(context.modifiers))) {
        events.push_back(std::move(*touch));
    }
}

} // namespace haylen::platform::sdl
