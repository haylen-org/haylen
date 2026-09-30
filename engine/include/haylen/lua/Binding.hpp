#pragma once

#include <lua.hpp>

#include <cstddef>
#include <exception>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>

#include "haylen/lua/FunctionTraits.hpp"
#include "haylen/lua/Stack.hpp"
#include "haylen/lua/Type.hpp"
#include "haylen/lua/Userdata.hpp"

namespace haylen::lua {

// Turns C++ functions, member functions and data members into Lua functions and registers modules.
class Binding final {
  public:
    // Runs `body` and turns a C++ exception into a Lua error with the same message. Lua errors raised inside `body` pass through untouched.
    template <typename Body> static int guarded(lua_State* L, Body&& body) {
        std::string message;
        try {
            return body();
        } catch (const std::exception& exception) {
            message = exception.what();
        }
        return luaL_error(L, "%s", message.c_str());
    }

    // Exposes a function whose parameters and result convert automatically.
    template <auto Function> static int function(lua_State* L) {
        using Traits = FunctionTraits<decltype(Function)>;
        using Arguments = typename Traits::Arguments;
        // clang-format off
        return guarded(L, [L] {
            auto arguments = readArguments<Arguments>(L, 1, std::make_index_sequence<std::tuple_size_v<Arguments>>{});
            return invokeAndPush<typename Traits::Result>(L, [&] { return std::apply(Function, arguments); });
        });
        // clang-format on
    }

    // Exposes a member function called with the object as its first Lua argument.
    template <auto Method> static int method(lua_State* L) {
        using Traits = FunctionTraits<decltype(Method)>;
        using Arguments = typename Traits::Arguments;
        // clang-format off
        return guarded(L, [L] {
            auto& self = Userdata::check<typename Traits::Class>(L, 1);
            auto arguments = readArguments<Arguments>(L, 2, std::make_index_sequence<std::tuple_size_v<Arguments>>{});
            return invokeAndPush<typename Traits::Result>(L, [&] {
                return std::apply([&](auto&&... values) -> decltype(auto) { return (self.*Method)(std::forward<decltype(values)>(values)...); }, arguments);
            });
        });
        // clang-format on
    }

    // Wraps a captureless callable taking the Lua state, so C++ exceptions become Lua errors.
    template <auto Callable> static int native(lua_State* L) {
        return guarded(L, [L] { return Callable(L); });
    }

    template <auto Member> static int getField(lua_State* L) {
        using Traits = FunctionTraits<decltype(Member)>;
        Stack::push(L, Userdata::check<typename Traits::Class>(L, 1).*Member);
        return 1;
    }

    // Setters receive the object, the key and the value, the way `__newindex` calls them.
    template <auto Member> static int setField(lua_State* L) {
        using Traits = FunctionTraits<decltype(Member)>;
        auto& self = Userdata::check<typename Traits::Class>(L, 1);
        self.*Member = Stack::read<typename Traits::Field>(L, 3);
        return 0;
    }

    template <auto Outer, auto Inner> static int getNestedField(lua_State* L) {
        using Traits = FunctionTraits<decltype(Outer)>;
        Stack::push(L, (Userdata::check<typename Traits::Class>(L, 1).*Outer).*Inner);
        return 1;
    }

    template <auto Outer, auto Inner> static int setNestedField(lua_State* L) {
        using OuterTraits = FunctionTraits<decltype(Outer)>;
        using InnerTraits = FunctionTraits<decltype(Inner)>;
        (Userdata::check<typename OuterTraits::Class>(L, 1).*Outer).*Inner = Stack::read<typename InnerTraits::Field>(L, 3);
        return 0;
    }

    // Registers a module of the engine that owns the state, so `require(name)` returns the table built by `opener`. Throws `std::runtime_error` when a module of Varn or an earlier registration already has the name.
    static void preload(lua_State* L, const char* name, lua_CFunction opener);

    // Creates a module table and fills it with the given functions.
    static void newModule(lua_State* L, const luaL_Reg* functions);

  private:
    template <typename Argument> static constexpr bool kMutableReference = std::is_lvalue_reference_v<Argument> && !std::is_const_v<std::remove_reference_t<Argument>> && Bound<std::remove_cvref_t<Argument>>;

    // Mutable references to bound objects alias the userdata, while every other argument is converted into its own value.
    template <typename Argument> using ArgumentHolder = std::conditional_t<kMutableReference<Argument>, Argument, std::remove_cvref_t<Argument>>;

    template <typename Argument> static ArgumentHolder<Argument> readArgument(lua_State* L, int index) {
        using Value = std::remove_cvref_t<Argument>;
        if constexpr (kMutableReference<Argument>) {
            return Userdata::check<Value>(L, index);
        } else {
            return Stack::read<Value>(L, index);
        }
    }

    // Braced initialization keeps the left-to-right order in which Lua arguments are read and validated.
    template <typename Tuple, std::size_t... Indices> static auto readArguments([[maybe_unused]] lua_State* L, [[maybe_unused]] int first, std::index_sequence<Indices...>) {
        using Holders = std::tuple<ArgumentHolder<std::tuple_element_t<Indices, Tuple>>...>;
        return Holders{readArgument<std::tuple_element_t<Indices, Tuple>>(L, first + static_cast<int>(Indices))...};
    }

    template <typename Result, typename Call> static int invokeAndPush(lua_State* L, Call&& call) {
        if constexpr (std::is_void_v<Result>) {
            call();
            return 0;
        } else {
            Stack::push(L, call());
            return 1;
        }
    }
};

} // namespace haylen::lua
