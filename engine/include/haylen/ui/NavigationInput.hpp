#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

#include "haylen/input/ActionMap.hpp"
#include "haylen/input/Input.hpp"
#include "haylen/input/VirtualInput.hpp"

namespace haylen::ui {

// The actions that move the UI focus, press the focused control, go back and open context menus: ui_accept, ui_cancel, ui_left, ui_right, ui_up, ui_down and ui_menu. An app remaps one by defining an action with its name in its action map, and the others keep their built-in bindings.
class NavigationInput final {
  public:
    enum class Action : std::uint8_t {
        Accept,
        Cancel,
        Left,
        Right,
        Up,
        Down,
        Menu,
    };

    static constexpr std::size_t kActionCount = 7;
    static constexpr std::array<std::string_view, kActionCount> kNames{"ui_accept", "ui_cancel", "ui_left", "ui_right", "ui_up", "ui_down", "ui_menu"};

    NavigationInput();

    // Reads every action from the app map when it defines it and from the built-in bindings otherwise. Blocked input, such as during a scene transition, holds every action up.
    void update(const input::ActionMap& actions, const input::Input& devices, const input::VirtualInput& virtualInput, bool blocked);

    [[nodiscard]] bool isDown(Action action) const noexcept {
        return down[static_cast<std::size_t>(action)];
    }
    [[nodiscard]] bool isPressed(Action action) const noexcept {
        return down[static_cast<std::size_t>(action)] && !wasDown[static_cast<std::size_t>(action)];
    }

    // The built-in bindings, which the documentation lists and a remap starts from.
    [[nodiscard]] const input::ActionMap& getDefaults() const noexcept {
        return defaults;
    }

  private:
    input::ActionMap defaults;
    input::Input idleInput;
    input::VirtualInput idleVirtualInput;
    std::array<bool, kActionCount> down{};
    std::array<bool, kActionCount> wasDown{};
};

} // namespace haylen::ui
