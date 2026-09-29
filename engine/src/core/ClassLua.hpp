#pragma once

#include <string_view>

struct lua_State;

namespace haylen::core {

// Provides haylen.class, the helper for app-wide Lua classes: named classes with inheritance, an init constructor, super, is checks, mixins and a readable __tostring.
class ClassLua final {
  public:
    // Pushes the class function, which is created once per Lua state.
    static void push(lua_State* L);

  private:
    static constexpr const char* kRegistryKey = "haylen.class";
    static const std::string_view kSource;
};

} // namespace haylen::core
