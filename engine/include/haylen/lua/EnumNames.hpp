#pragma once

#include <concepts>
#include <optional>
#include <string_view>
#include <type_traits>

namespace haylen::lua {

// Enums cross the boundary as strings. Specializations provide the conversion both ways.
template <typename T> struct EnumNames;

template <typename T>
concept NamedEnum = std::is_enum_v<T> && requires(std::string_view name, T value) {
    { EnumNames<T>::fromName(name) } -> std::same_as<std::optional<T>>;
    { EnumNames<T>::name(value) } -> std::convertible_to<std::string_view>;
};

} // namespace haylen::lua
