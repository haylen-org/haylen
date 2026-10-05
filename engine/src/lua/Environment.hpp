#pragma once

#include <lua.hpp>

#include <string>
#include <string_view>
#include <vector>

#include "haylen/lua/Error.hpp"

namespace haylen::core {
class Engine;
struct AppConfig;
} // namespace haylen::core

namespace haylen::io {
class Package;
} // namespace haylen::io

namespace haylen::lua {

// Prepares the Lua state of an engine for app scripts.
class Environment final {
  public:
    // Binds the Lua state to the engine and makes `require` load modules from the package instead of the host file system: the modules of the app from its source folder, and the modules of every plugin that `app.json` lists from `plugins/<id>/source`. The failures of `async` tasks and `ffi` callbacks reach the error screen. App code loads chunks only as text, has no `string.dump`, and cannot reach the protected metatables of the engine or the upvalues of native functions through the debug library.
    static void install(core::Engine& engine, lua_State* L);

    // Throws `std::runtime_error`, naming both files, when a module of the app has the name of a plugin module, which `require` would never load.
    static void checkModules(core::Engine& engine);

    // Loads a module of the package and runs it, which is how the app starts with `source/main.lua`. Throws `Error` with the message and the stack when loading or running fails.
    static void runModule(lua_State* L, const io::Package& package, const std::string& path);

  private:
    static constexpr int kLoadMode = 3;
    static constexpr int kLoadFileMode = 2;

    [[nodiscard]] static std::string modulePath(std::string_view name);
    [[nodiscard]] static std::vector<std::string> getCandidates(const core::AppConfig& config, std::string_view name);
    [[nodiscard]] static std::vector<Error::Frame> readFrames(lua_State* L, int index);
    [[nodiscard]] static Error::Frame readFrame(lua_State* L, int index);
    [[nodiscard]] static std::string readText(lua_State* L, int index, const char* key);
    [[nodiscard]] static int readInteger(lua_State* L, int index, const char* key);
    static int reportFailure(lua_State* L);

    // Loads a module of the package, named after its package path, and leaves its function on the stack, or the message of Lua when it fails, and returns the status of Lua. A module that the authenticated catalog of a protected release holds as Lua bytecode loads as bytecode, whose bytes are wiped once Lua read them, and every other module loads as text, so no other file ever reaches Lua as bytecode.
    static int loadModule(lua_State* L, const io::Package& package, const std::string& path);
    static void installFailureHandler(lua_State* L);
    static int searchPackage(lua_State* L);

    // Calls the original `load` or `loadfile`, held as the first upvalue, with the mode at the index of the second upvalue forced to text.
    static int loadAsText(lua_State* L);
    static int doFileAsText(lua_State* L);
    static int finishDoFile(lua_State* L, int status, lua_KContext context);
    static void wrapLoader(lua_State* L, const char* name, int modeIndex);
    static void restrictLoading(lua_State* L);

    static int setUnprotectedMetatable(lua_State* L);
    // Calls the original `getupvalue` or `setupvalue`, held as the upvalue, for Lua functions only.
    static int accessScriptUpvalue(lua_State* L);
    static void restrictDebug(lua_State* L);
};

} // namespace haylen::lua
