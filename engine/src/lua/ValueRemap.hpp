#pragma once

#include <lua.hpp>

#include <unordered_set>

namespace haylen::lua {

// One walk of the Lua heap after a reload, from the registry through every table, closure, user value, metatable and coroutine stack it reaches, that puts the new functions where the old ones were and the live tables where the staging tables of the reload were, so callbacks the engine holds, copies in other modules and copied metamethods run the new code. It reads and writes raw values only, so it never runs a metamethod.
class ValueRemap final {
  public:
    // Replaces every value that is a key of the table at `replacements` by its value, leaves the tables at `skipped` and `classes` alone, and pushes a list of the tables whose metatable is a key of the table at `classes` or a class whose `super` chain reaches one.
    static void run(lua_State* L, int replacements, int skipped, int classes);

  private:
    static constexpr int kMaxSuperDepth = 32;

    ValueRemap(lua_State* state, int replacementsIndex, int classesIndex, int workIndex, int foundIndex) noexcept : L(state), replacements(replacementsIndex), classes(classesIndex), work(workIndex), found(foundIndex) {}

    void enqueue(int index);

    // Pushes the replacement of the value at index and returns `true`, or pushes nothing and returns `false`.
    [[nodiscard]] bool pushReplacement(int index);
    [[nodiscard]] bool isInstanceOfPatched(int metatable);

    void visitTable(int index);
    void visitFunction(int index);
    void visitUserdata(int index);
    void visitThread(int index);

    lua_State* L;
    int replacements;
    int classes;
    int work;
    int found;
    int pending = 0;
    int foundCount = 0;
    std::unordered_set<const void*> seen;
};

} // namespace haylen::lua
