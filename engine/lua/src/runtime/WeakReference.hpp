#pragma once

#include <lua.hpp>

namespace haylen::lua {

// Refers to a Lua table or userdata without keeping it alive. Native code checks it before touching the value, so it never writes to an object the collector has taken. It must be destroyed while the Lua state is open.
class WeakReference final {
  public:
    WeakReference(lua_State* L, int index);
    ~WeakReference();

    WeakReference(const WeakReference&) = delete;
    WeakReference& operator=(const WeakReference&) = delete;

    // Pushes the value and returns `true`, or pushes nothing and returns `false` once it was collected.
    [[nodiscard]] bool push(lua_State* L) const;
    [[nodiscard]] bool isAlive() const;

    // Returns the identity of the value, which stays unique while it is alive.
    [[nodiscard]] const void* getIdentity() const noexcept {
        return identity;
    }

  private:
    static constexpr const char* kTable = "haylen.weakReferences";

    static void pushTable(lua_State* L);

    lua_State* state;
    lua_Integer key = 0;
    const void* identity;
};

} // namespace haylen::lua
