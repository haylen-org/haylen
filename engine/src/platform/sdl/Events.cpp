#include "platform/sdl/Events.hpp"

#include <algorithm>
#include <cstring>

#include <SDL3/SDL_stdinc.h>

#include "platform/sdl/Keys.hpp"

namespace haylen::platform::sdl {

void Events::translate(const SDL_Event& source, const Context& context, std::vector<platform::Event>& events) {
    const input::KeyModifiers held = toModifiers(context.modifiers);
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
    case SDL_EVENT_MOUSE_BUTTON_UP: {
        // Buttons beyond the three the engine names, such as the back and forward buttons of a mouse, reach the app as nothing.
        const std::optional<input::MouseButton> button = toMouseButton(source.button.button);
        if (!button) {
            return;
        }
        const platform::Event::Type type = source.type == SDL_EVENT_MOUSE_BUTTON_DOWN ? platform::Event::Type::MouseDown : platform::Event::Type::MouseUp;
        events.push_back({.type = type, .modifiers = held, .mouseButton = *button, .position = {source.button.x * context.density, source.button.y * context.density}});
        return;
    }
    case SDL_EVENT_MOUSE_MOTION:
        events.push_back({.type = platform::Event::Type::MouseMove, .modifiers = held, .position = {source.motion.x * context.density, source.motion.y * context.density}, .delta = {source.motion.xrel * context.density, source.motion.yrel * context.density}});
        return;
    case SDL_EVENT_MOUSE_WHEEL:
        // The wheels of SDL scroll right with positive steps, while the engine scrolls left with them.
        events.push_back({.type = platform::Event::Type::MouseScroll, .modifiers = held, .scroll = {-source.wheel.x, source.wheel.y}});
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

    if (source.type >= SDL_EVENT_WINDOW_FIRST && source.type <= SDL_EVENT_WINDOW_LAST) {
        if (const std::optional<platform::Event> window = translateWindow(source)) {
            events.push_back(*window);
        }
        return;
    }
    if (const std::optional<platform::Event> app = translateApp(source)) {
        events.push_back(*app);
    }
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
    return platform::Event{.type = type, .key = key, .repeat = source.repeat, .modifiers = toModifiers(source.mod)};
}

// A desktop window that comes back from the taskbar resumes the app, while a window that only leaves its maximized size does not.
std::optional<platform::Event> Events::translateWindow(const SDL_Event& source) noexcept {
    switch (source.type) {
    case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
    case SDL_EVENT_WINDOW_DISPLAY_SCALE_CHANGED:
    case SDL_EVENT_WINDOW_METAL_VIEW_RESIZED:
        return platform::Event{.type = platform::Event::Type::Resized};
    case SDL_EVENT_WINDOW_FOCUS_GAINED:
        return kAppStates ? std::nullopt : std::optional<platform::Event>({.type = platform::Event::Type::FocusGained});
    case SDL_EVENT_WINDOW_FOCUS_LOST:
        return kAppStates ? std::nullopt : std::optional<platform::Event>({.type = platform::Event::Type::FocusLost});
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

// Every touch of the screen travels with each change, and the finger that changed is marked. A finger that lifts travels one last time before it leaves the list.
void Events::appendTouch(const SDL_TouchFingerEvent& source, const Context& context, std::vector<platform::Event>& events) {
    const math::Vec2 position{source.x * context.framebuffer.x, source.y * context.framebuffer.y};
    Finger* const end = fingers.data() + fingerCount;
    Finger* finger = std::find_if(fingers.data(), end, [&source](const Finger& candidate) { return candidate.id == source.fingerID; });
    if (finger == end) {
        if (source.type != SDL_EVENT_FINGER_DOWN || fingerCount == fingers.size()) {
            return;
        }
        ++fingerCount;
    }
    *finger = {.id = source.fingerID, .position = position};

    platform::Event::Type type = platform::Event::Type::TouchMoved;
    if (source.type == SDL_EVENT_FINGER_DOWN) {
        type = platform::Event::Type::TouchBegan;
    } else if (source.type == SDL_EVENT_FINGER_UP) {
        type = platform::Event::Type::TouchEnded;
    } else if (source.type == SDL_EVENT_FINGER_CANCELED) {
        type = platform::Event::Type::TouchCancelled;
    }
    platform::Event event{.type = type, .modifiers = toModifiers(context.modifiers), .touchCount = fingerCount};
    for (std::size_t index = 0; index < fingerCount; ++index) {
        event.touches[index] = {.id = static_cast<std::uint64_t>(fingers[index].id), .position = fingers[index].position, .changed = &fingers[index] == finger};
    }
    events.push_back(event);

    if (type == platform::Event::Type::TouchEnded || type == platform::Event::Type::TouchCancelled) {
        *finger = fingers[--fingerCount];
    }
}

} // namespace haylen::platform::sdl
