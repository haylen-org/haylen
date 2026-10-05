#pragma once

#include <lua.hpp>

#include <functional>
#include <map>
#include <span>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

#include "haylen/lua/Reference.hpp"

namespace haylen::lua {

// The Lua functions of one module file that some values reach, through the fields of mergeable tables and their metatables and through the variables those functions capture, together with the variables they capture by name. A reload joins the variables of the new functions to the ones of the old functions with the same names, so both share the state of the module.
class ModuleClosures final {
  public:
    // A variable that a function captures, by the place of the function in the collection and the number of the variable.
    struct Cell {
        int closure = 0;
        int upvalue = 0;
        const void* id = nullptr;
    };

    // Collects the functions of the file at `path` that the values at the indices reach. The value of each index may be `nil`.
    ModuleClosures(lua_State* L, std::string_view path, std::span<const int> roots);

    // The cells of every variable name, without `_ENV`, which every function of a chunk shares with the chunk.
    [[nodiscard]] const std::map<std::string, std::vector<Cell>, std::less<>>& getCells() const noexcept {
        return cells;
    }

    // Returns a variable name that two different variables of the functions have, which no reload can tell apart, or an empty text.
    [[nodiscard]] std::string findAmbiguous() const;

    void pushClosure(lua_State* L, const Cell& cell) const;

    // Pushes a table of the value of every captured variable by name.
    void pushValues(lua_State* L) const;

  private:
    static constexpr int kMaxDepth = 64;

    void visit(lua_State* L, int index, int depth);
    void addClosure(lua_State* L, int index, int depth);

    std::string source;
    Reference anchor;
    int count = 0;
    std::unordered_set<const void*> seen;
    std::map<std::string, std::vector<Cell>, std::less<>> cells;
};

} // namespace haylen::lua
