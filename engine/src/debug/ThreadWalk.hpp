#pragma once

#include <lua.hpp>

#include <functional>
#include <unordered_set>

namespace haylen::debug {

// One walk of the Lua heap from the registry through every table, closure, user value, metatable and coroutine stack it reaches, which hands over every coroutine it finds, since Lua sets its hooks per coroutine and lists none of them. It reads raw values only, so it never runs a metamethod, and it holds the collector, so no finalizer runs during it.
class ThreadWalk final {
  public:
    // Calls the visitor with every coroutine that the heap holds other than the one that walks, which the caller reaches already.
    static void run(lua_State* L, const std::function<void(lua_State* thread)>& visit);

  private:
    ThreadWalk(lua_State* L, int workIndex) noexcept : state(L), work(workIndex) {}

    void enqueue(int index);
    void visitTable(int index);
    void visitFunction(int index);
    void visitUserdata(int index);
    void visitThread(lua_State* thread);

    lua_State* state;
    int work;
    int pending = 0;
    std::unordered_set<const void*> seen;
};

} // namespace haylen::debug
