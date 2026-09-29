#pragma once

#include <string>
#include <string_view>

struct lua_State;

namespace haylen::core {
class Engine;
}

namespace haylen::lua {

// Prepares the Lua state of an engine for app scripts.
class Environment final {
  public:
    // Binds the Lua state to the engine and makes require load modules from the source folder of the package instead of the host file system.
    static void install(core::Engine& engine, lua_State* L);

  private:
    static const std::string_view kTaskErrors;

    [[nodiscard]] static std::string modulePath(std::string_view name);
    static int reportTaskError(lua_State* L);
    static void installTaskErrors(lua_State* L);
    static int searchPackage(lua_State* L);
};

} // namespace haylen::lua
