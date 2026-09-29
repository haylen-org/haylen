#pragma once

#include <lua.hpp>

#include <array>
#include <cstdint>
#include <optional>
#include <string_view>
#include <type_traits>

#include "haylen/math/Color.hpp"
#include "haylen/math/Vec2.hpp"

namespace haylen::lua {

// A number, Vec2 or Color field of a bound type that native code reads and writes without running Lua, such as the position of a sprite. ClassBuilder records one for every field of those types, and tweens use them to animate userdata natively. The functions take the userdata block and return false when the object behind it was released.
struct NativeProperty {
    enum class Kind : std::uint8_t {
        Number,
        Vector,
        Color,
    };

    using Values = std::array<float, 4>;
    using Reader = bool (*)(void* storage, Values& values);
    using Writer = bool (*)(void* storage, const Values& values);

    Kind kind = Kind::Number;
    Reader read = nullptr;
    Writer write = nullptr;

    // Returns the kind a field type has as a native property, or nothing for types that cannot be one.
    template <typename Field> [[nodiscard]] static constexpr std::optional<Kind> kindOf() noexcept {
        if constexpr (std::is_same_v<Field, float> || std::is_same_v<Field, double>) {
            return Kind::Number;
        } else if constexpr (std::is_same_v<Field, math::Vec2>) {
            return Kind::Vector;
        } else if constexpr (std::is_same_v<Field, math::Color>) {
            return Kind::Color;
        } else {
            return std::nullopt;
        }
    }

    template <typename Field> static void pack(const Field& value, Values& values) noexcept {
        if constexpr (std::is_same_v<Field, math::Vec2>) {
            values = {value.x, value.y, 0.0F, 0.0F};
        } else if constexpr (std::is_same_v<Field, math::Color>) {
            values = {value.r, value.g, value.b, value.a};
        } else {
            values = {static_cast<float>(value), 0.0F, 0.0F, 0.0F};
        }
    }

    template <typename Field> [[nodiscard]] static Field unpack(const Values& values) noexcept {
        if constexpr (std::is_same_v<Field, math::Vec2>) {
            return {values[0], values[1]};
        } else if constexpr (std::is_same_v<Field, math::Color>) {
            return {values[0], values[1], values[2], values[3]};
        } else {
            return static_cast<Field>(values[0]);
        }
    }

    // Returns the property of the userdata at index, or null when it is not a userdata or its type records no such property.
    [[nodiscard]] static const NativeProperty* find(lua_State* L, int index, std::string_view name);

    // Records the property in the metatable at index, the one ClassBuilder is building. The property must outlive the Lua state.
    static void record(lua_State* L, int metatable, const char* name, NativeProperty& property);

  private:
    static constexpr const char* kField = "__native";
};

} // namespace haylen::lua
