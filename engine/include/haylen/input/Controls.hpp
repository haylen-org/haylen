#pragma once

#include <array>
#include <cstddef>
#include <optional>
#include <string_view>
#include <utility>

#include "haylen/input/GamepadAxis.hpp"
#include "haylen/input/GamepadButton.hpp"
#include "haylen/input/Key.hpp"
#include "haylen/input/MouseButton.hpp"

namespace haylen::input {

// The keys, mouse buttons, gamepad buttons and gamepad axes that input tracks, with the names bindings and Lua use for them, such as "leftShift", "middle", "south" and "leftX".
class Controls final {
  public:
    static constexpr std::size_t kKeyCount = 349;
    static constexpr std::size_t kMouseButtonCount = 3;
    static constexpr std::size_t kGamepadButtonCount = 15;
    static constexpr std::size_t kGamepadAxisCount = 6;

    [[nodiscard]] static std::optional<Key> keyFromName(std::string_view name) noexcept;
    [[nodiscard]] static std::string_view keyName(Key key) noexcept;
    [[nodiscard]] static std::optional<MouseButton> mouseButtonFromName(std::string_view name) noexcept;
    [[nodiscard]] static std::string_view mouseButtonName(MouseButton button) noexcept;
    [[nodiscard]] static std::optional<GamepadButton> gamepadButtonFromName(std::string_view name) noexcept;
    [[nodiscard]] static std::string_view gamepadButtonName(GamepadButton button) noexcept;
    [[nodiscard]] static std::optional<GamepadAxis> gamepadAxisFromName(std::string_view name) noexcept;
    [[nodiscard]] static std::string_view gamepadAxisName(GamepadAxis axis) noexcept;

  private:
    static const std::array<std::pair<std::string_view, Key>, 120> kKeyNames;
    static const std::array<std::string_view, kMouseButtonCount> kMouseButtonNames;
    static const std::array<std::string_view, kGamepadButtonCount> kGamepadButtonNames;
    static const std::array<std::string_view, kGamepadAxisCount> kGamepadAxisNames;

    template <typename Enum, std::size_t Size> [[nodiscard]] static std::optional<Enum> findIndexed(const std::array<std::string_view, Size>& names, std::string_view name) noexcept;
};

} // namespace haylen::input
