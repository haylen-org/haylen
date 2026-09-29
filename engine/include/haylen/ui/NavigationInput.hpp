#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>
#include <vector>

#include "haylen/input/ActionMap.hpp"
#include "haylen/input/Input.hpp"
#include "haylen/input/VirtualInput.hpp"

namespace haylen::ui {

// The actions that move the UI focus, press the focused control, go back and open context menus: uiAccept, uiCancel, uiLeft, uiRight, uiUp, uiDown and uiMenu. An app remaps one by defining an action with its name in its action map, and the others keep their built-in bindings.
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
    static constexpr std::array<std::string_view, kActionCount> kNames{"uiAccept", "uiCancel", "uiLeft", "uiRight", "uiUp", "uiDown", "uiMenu"};

    NavigationInput();

    // Reads every action with the bindings the app map gives it when it defines it and with the built-in bindings otherwise. The UI reads the presses it captures from the app map itself, and blocked input, such as during a scene transition, holds every action up until it is released, like the actions of the app map.
    void update(const input::ActionMap& actions, const input::Input& devices, const input::VirtualInput& virtualInput, bool blocked);

    // The bindings an action reads, remapped by the app or built in.
    [[nodiscard]] const std::vector<input::ActionMap::Binding>& getBindings(Action action) const;

    [[nodiscard]] bool isDown(Action action) const noexcept {
        return down[static_cast<std::size_t>(action)];
    }
    [[nodiscard]] bool isPressed(Action action) const noexcept {
        return down[static_cast<std::size_t>(action)] && !wasDown[static_cast<std::size_t>(action)];
    }

  private:
    input::ActionMap defaults;
    input::ActionMap resolved;
    std::array<bool, kActionCount> down{};
    std::array<bool, kActionCount> wasDown{};
};

} // namespace haylen::ui
