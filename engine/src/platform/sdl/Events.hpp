#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

#include <SDL3/SDL_events.h>

#include "haylen/math/Vec2.hpp"
#include "haylen/platform/Event.hpp"

#if defined(__APPLE__)
#include <TargetConditionals.h>
#endif

namespace haylen::platform::sdl {

// Turns the events of SDL into the events of the engine, with positions in framebuffer pixels. It follows the fingers on the screen, because SDL reports each finger on its own while the engine hands every touch of the screen with each change, whether the window is minimized and the state of the app.
class Events final {
  public:
    // What the translation reads from the window and the keyboard: the framebuffer pixels per unit of the coordinates of the window, the size of the framebuffer, which the positions of fingers are fractions of, and the modifier keys held, which only the events of keys carry.
    struct Context {
        float density = 1.0F;
        math::Vec2 framebuffer;
        SDL_Keymod modifiers = SDL_KMOD_NONE;
    };

    // Appends the events of the engine that the event of SDL stands for, which is none for the events the engine does not use and one per character of a text.
    void translate(const SDL_Event& source, const Context& context, std::vector<platform::Event>& events);

    // Whether the event is a press or a release of the back button of the platform, the Menu button of the Apple TV remote.
    [[nodiscard]] static bool isPlatformBack(const SDL_Event& source) noexcept;

  private:
    struct Finger {
        SDL_FingerID id = 0;
        math::Vec2 position;
    };

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
    [[nodiscard]] static std::optional<platform::Event> translateKey(const SDL_KeyboardEvent& source) noexcept;
    [[nodiscard]] std::optional<platform::Event> translateWindow(const SDL_Event& source) noexcept;
    [[nodiscard]] std::optional<platform::Event> translateApp(const SDL_Event& source) noexcept;
    static void appendText(const char* text, const Context& context, std::vector<platform::Event>& events);
    void appendTouch(const SDL_TouchFingerEvent& source, const Context& context, std::vector<platform::Event>& events);

    std::array<Finger, platform::Event::kMaxTouchPoints> fingers{};
    std::size_t fingerCount = 0;
    bool minimized = false;

    // The last state of a UIKit app, which an app that starts hears as active.
    platform::Event::Type appState = platform::Event::Type::FocusGained;
};

} // namespace haylen::platform::sdl
