#pragma once

#include <tuple>

namespace haylen::lua {

// Describes the class, result, arguments or field type of a function, member function or data member pointer.
template <typename F> struct FunctionTraits;

template <typename R, typename... Args> struct FunctionTraits<R (*)(Args...)> {
    using Result = R;
    using Arguments = std::tuple<Args...>;
};

template <typename R, typename... Args> struct FunctionTraits<R (*)(Args...) noexcept> : FunctionTraits<R (*)(Args...)> {};

template <typename C, typename R, typename... Args> struct FunctionTraits<R (C::*)(Args...)> {
    using Class = C;
    using Result = R;
    using Arguments = std::tuple<Args...>;
};

template <typename C, typename R, typename... Args> struct FunctionTraits<R (C::*)(Args...) const> : FunctionTraits<R (C::*)(Args...)> {};
template <typename C, typename R, typename... Args> struct FunctionTraits<R (C::*)(Args...) noexcept> : FunctionTraits<R (C::*)(Args...)> {};
template <typename C, typename R, typename... Args> struct FunctionTraits<R (C::*)(Args...) const noexcept> : FunctionTraits<R (C::*)(Args...)> {};

template <typename C, typename T> struct FunctionTraits<T C::*> {
    using Class = C;
    using Field = T;
};

} // namespace haylen::lua
