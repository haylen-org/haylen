#pragma once

#include <cstddef>
#include <functional>
#include <string>
#include <string_view>
#include <unordered_map>

#include "haylen/math/Vec2.hpp"

namespace haylen::input {

// Named buttons and sticks written by on-screen controls and read by the action map.
class VirtualInput final {
  public:
    void setButton(std::string_view name, bool down);
    void setStick(std::string_view name, math::Vec2 value);
    void clear() noexcept;

    [[nodiscard]] bool isButtonDown(std::string_view name) const noexcept;
    [[nodiscard]] math::Vec2 getStick(std::string_view name) const noexcept;

  private:
    // Hashes names as views, so the action map looks them up every frame without building strings.
    struct NameHash {
        using is_transparent = void;

        [[nodiscard]] std::size_t operator()(std::string_view name) const noexcept {
            return std::hash<std::string_view>{}(name);
        }
    };

    std::unordered_map<std::string, bool, NameHash, std::equal_to<>> buttons;
    std::unordered_map<std::string, math::Vec2, NameHash, std::equal_to<>> sticks;
};

} // namespace haylen::input
