#pragma once

#include <lua.hpp>

namespace haylen::lua {

// Keeps a Lua value alive in the registry for as long as the reference lives. Its state is the main thread, where deferred callbacks run. References must be released while the Lua state is still open.
class Reference final {
  public:
    Reference() = default;
    Reference(lua_State* L, int index);
    ~Reference();

    Reference(const Reference&) = delete;
    Reference& operator=(const Reference&) = delete;
    Reference(Reference&& other) noexcept;
    Reference& operator=(Reference&& other) noexcept;

    void push(lua_State* L) const;
    void reset() noexcept;
    [[nodiscard]] bool isValid() const noexcept {
        return reference != LUA_NOREF;
    }
    [[nodiscard]] lua_State* getState() const noexcept {
        return state;
    }

  private:
    lua_State* state = nullptr;
    int reference = LUA_NOREF;
};

} // namespace haylen::lua
