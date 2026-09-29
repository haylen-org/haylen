#pragma once

#include <lua.hpp>

#include <string>
#include <string_view>

namespace haylen::core {
class Engine;
}

namespace haylen::lua {

// Prepares the Lua state of an engine for app scripts.
class Environment final {
  public:
    // Binds the Lua state to the engine and makes require load modules from the source folder of the package instead of the host file system. App code loads chunks only as text, has no string.dump, and cannot reach the protected metatables of the engine or the upvalues of native functions through the debug library.
    static void install(core::Engine& engine, lua_State* L);

  private:
    static const std::string_view kTaskErrors;
    static constexpr int kLoadMode = 3;
    static constexpr int kLoadFileMode = 2;

    [[nodiscard]] static std::string modulePath(std::string_view name);
    static int reportTaskError(lua_State* L);
    static void installTaskErrors(lua_State* L);
    static int searchPackage(lua_State* L);

    // Calls the original load or loadfile, held as the first upvalue, with the mode at the index of the second upvalue forced to text.
    static int loadAsText(lua_State* L);
    static int doFileAsText(lua_State* L);
    static int finishDoFile(lua_State* L, int status, lua_KContext context);
    static void wrapLoader(lua_State* L, const char* name, int modeIndex);
    static void restrictLoading(lua_State* L);

    static int setUnprotectedMetatable(lua_State* L);
    // Calls the original getupvalue or setupvalue, held as the upvalue, for Lua functions only.
    static int accessScriptUpvalue(lua_State* L);
    static void restrictDebug(lua_State* L);
};

} // namespace haylen::lua
