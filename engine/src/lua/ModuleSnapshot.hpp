#pragma once

#include <lua.hpp>

#include <string_view>

namespace haylen::lua {

// What a module produced when it last loaded, which the next reload compares with the live state to tell the values the app changed from the literals the developer edited: its value, the variables its functions capture by name and the globals it wrote. Mergeable tables are copied, within a bound on their depth and entries, and every other value is kept as it is.
class ModuleSnapshot final {
  public:
    // Tells whether a reload merges the fields of the table at index into the live table: a table without a metatable, or a prototype whose `__index` is the table itself, such as a class of `haylen.class`. Any other table, such as an instance of a class, is an object whose identity a reload keeps.
    [[nodiscard]] static bool isMergeable(lua_State* L, int index);

    // Pushes the snapshot `{value, upvalues, globals}` of the module value and the tables of captured values by name and of written globals at the indices. Every table it copies that no module claimed yet becomes a table of the module at `path` in the weak table at `creators`, so a later reload tells the tables of the module from the tables of other modules.
    static void push(lua_State* L, int value, int upvalues, int globals, int creators, std::string_view path);

    // Tells whether the live value still equals the value that a snapshot holds, comparing copied tables by their contents.
    [[nodiscard]] static bool equals(lua_State* L, int live, int base);

  private:
    static constexpr int kMaxDepth = 32;
    static constexpr int kMaxEntries = 20000;

    // Pushes a copy of the value at index, reusing the copies that the table at `copies` already holds for shared and cyclic tables.
    static void copy(lua_State* L, int value, int copies, int depth, int& budget);
    static void claim(lua_State* L, int copies, int creators, std::string_view path);
    [[nodiscard]] static bool compare(lua_State* L, int live, int base, int visited, int depth, int& budget);
};

} // namespace haylen::lua
