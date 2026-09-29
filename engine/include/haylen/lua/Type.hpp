#pragma once

#include <concepts>
#include <memory>

namespace haylen::lua {

// Describes how a C++ type lives in Lua as userdata. Specializations provide the metatable name and the stored representation: the value itself, a shared pointer to it, or a weak pointer to an object that something else owns, which Lua sees as released once its owner destroys it.
template <typename T> struct Type;

template <typename T>
concept Bound = requires {
    { Type<T>::name } -> std::convertible_to<const char*>;
    typename Type<T>::Storage;
};

template <typename T>
concept SharedBound = Bound<T> && std::same_as<typename Type<T>::Storage, std::shared_ptr<T>>;

template <typename T>
concept WeakBound = Bound<T> && std::same_as<typename Type<T>::Storage, std::weak_ptr<T>>;

template <typename T>
concept ValueBound = Bound<T> && std::same_as<typename Type<T>::Storage, T>;

} // namespace haylen::lua
