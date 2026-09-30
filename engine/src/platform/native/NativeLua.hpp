#pragma once

#include <lua.hpp>

#include <array>
#include <memory>
#include <string_view>

#include "haylen/lua/Type.hpp"
#include "platform/native/NativeCallback.hpp"

namespace haylen::lua {

template <> struct Type<platform::NativeCallback> {
    static constexpr const char* name = "haylen.NativeCallback";
    using Storage = std::shared_ptr<platform::NativeCallback>;
};

} // namespace haylen::lua

namespace haylen::platform {

// Installs `haylen.native`, which loads native libraries for Varn's `ffi`, finds their symbols and creates callbacks that native code may call from any thread.
class NativeLua final {
  public:
    static void install(lua_State* L);

  private:
    static constexpr std::array<std::string_view, 2> kLoadOptions{"init", "global"};
    static constexpr std::array<std::string_view, 1> kCallbackOptions{"thread"};

    static int available(lua_State* L);
    static int load(lua_State* L);
    static int findSymbol(lua_State* L);
    static int callback(lua_State* L);
    static int getPointer(lua_State* L);
    static int isFreed(lua_State* L);
    static int freeCallback(lua_State* L);
    static int open(lua_State* L);

    // Pushes the `ffi` module of Varn, which turns loaded libraries into callable namespaces.
    static void pushFfi(lua_State* L);
    [[nodiscard]] static std::shared_ptr<NativeCallback>& getStorage(lua_State* L);
};

} // namespace haylen::platform
