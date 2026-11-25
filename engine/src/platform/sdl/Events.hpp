#pragma once

#include <bitset>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

#include <SDL3/SDL_events.h>

#include "haylen/math/Vec2.hpp"
#include "haylen/platform/Event.hpp"
#include "platform/Touches.hpp"

#if defined(__APPLE__)
#include <TargetConditionals.h>
#endif

namespace haylen::platform::sdl {

// Turns the events of SDL into the events of the engine, with positions in framebuffer pixels. It follows the fingers on the screen, which SDL reports each on its own, whether the window is minimized and the state of the app.
class Events final {
  public:
    // What the translation reads from the window and the keyboard: the framebuffer pixels per unit of the coordinates of the window, the size of the framebuffer, which the positions of fingers are fractions of, the modifier keys held, which only the events of keys carry, and whether a window of the app, its own or the window of one of its outputs, holds the keyboard.
    struct Context {
        float density = 1.0F;
        math::Vec2 framebuffer{};
        SDL_Keymod modifiers = SDL_KMOD_NONE;
        bool appHoldsKeyboard = false;
    };

    // Appends the events of the engine that the event of SDL stands for, which is none for the events the engine does not use and one per character of a text.
    void translate(const SDL_Event& source, const Context& context, std::vector<platform::Event>& events);

    // The event of the engine that a press, a release, a move or a wheel of the mouse stands for, with its positions in the units of the window times the scale, or nothing for the other events and for a button that the engine does not name. The window of the app and the windows of its outputs translate the pointer through it alike.
    [[nodiscard]] static std::optional<platform::Event> translatePointer(const SDL_Event& source, float scale, SDL_Keymod modifiers) noexcept;

    // Whether the event is a press or a release of the back button of the platform, the Menu button of the Apple TV remote.
    [[nodiscard]] static bool isPlatformBack(const SDL_Event& source) noexcept;

    // The window that an event of a window, the keyboard, text, the mouse or a finger happened in, or 0 for an event that belongs to no window, such as one of a controller or of the app.
    [[nodiscard]] static SDL_WindowID getWindowId(const SDL_Event& source) noexcept;

    // Whether the event happened in a window that SDL destroyed since, whose events it delivers after the window went away.
    [[nodiscard]] static bool isOfClosedWindow(const SDL_Event& source) noexcept;

    // Whether the app heard the key go down and not come up yet, so its release reaches the app wherever the keyboard went since.
    [[nodiscard]] bool isHeld(SDL_Scancode key) const noexcept {
        return key < SDL_SCANCODE_COUNT && held.test(static_cast<std::size_t>(key));
    }

  private:
    // The Apple TV remote reports its touch surface as touches, which the gamepad of the remote stands for instead. Apps of UIKit hear the states of the app, while desktop windows minimize.
#if defined(__APPLE__) && TARGET_OS_TV
    static constexpr bool kRemote = true;
#else
    static constexpr bool kRemote = false;
#endif
#if defined(__APPLE__) && !TARGET_OS_OSX
    static constexpr bool kAppStates = true;
#else
    static constexpr bool kAppStates = false;
#endif

    [[nodiscard]] static input::KeyModifiers toModifiers(SDL_Keymod modifiers) noexcept;
    [[nodiscard]] static std::optional<input::MouseButton> toMouseButton(std::uint8_t button) noexcept;
    [[nodiscard]] std::optional<platform::Event> translateKey(const SDL_KeyboardEvent& source) noexcept;
    [[nodiscard]] std::optional<platform::Event> translateWindow(const SDL_Event& source, const Context& context) noexcept;
    [[nodiscard]] std::optional<platform::Event> translateFocus(bool gained, const Context& context) noexcept;
    [[nodiscard]] std::optional<platform::Event> translateApp(const SDL_Event& source) noexcept;
    static void appendText(const char* text, const Context& context, std::vector<platform::Event>& events);
    void appendTouch(const SDL_TouchFingerEvent& source, const Context& context, std::vector<platform::Event>& events);

    Touches touches;
    bool minimized = false;
    std::bitset<SDL_SCANCODE_COUNT> held;

    // Whether the app has the focus, as the app heard it last, which it has not heard yet before the first change of the focus.
    std::optional<bool> focused;

    // The last state of a UIKit app, which an app that starts hears as active.
    platform::Event::Type appState = platform::Event::Type::FocusGained;
};

} // namespace haylen::platform::sdl
