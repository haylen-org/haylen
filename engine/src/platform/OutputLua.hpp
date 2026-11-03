#pragma once

#include <array>
#include <memory>
#include <string_view>

#include "haylen/lua/Type.hpp"
#include "haylen/platform/Output.hpp"

struct lua_State;

namespace haylen::lua {

template <> struct Type<platform::Output> {
    static constexpr const char* name = "haylen.Output";
    using Storage = std::shared_ptr<platform::Output>;
};

} // namespace haylen::lua

namespace haylen::platform {

// Installs the `Output` class of `haylen.display`, an output of the app on another display, and opens one with `display.open(id, options)`.
class OutputLua final {
  public:
    static void install(lua_State* L);
    static int open(lua_State* L);

  private:
    static constexpr std::array<std::string_view, 5> kOptions{"fullscreen", "width", "height", "title", "clearColor"};

    static int id(lua_State* L);
    static int display(lua_State* L);
    static int state(lua_State* L);
    static int shown(lua_State* L);
    static int ending(lua_State* L);
    static int width(lua_State* L);
    static int height(lua_State* L);
    static int scale(lua_State* L);
    static int target(lua_State* L);
    static int clearColor(lua_State* L);
    static int setClearColor(lua_State* L);
    static int close(lua_State* L);
};

} // namespace haylen::platform
