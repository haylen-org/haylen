#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "haylen/3d/navigation/NavMesh.hpp"
#include "haylen/3d/navigation/QueryFilter.hpp"
#include "haylen/lua/Type.hpp"

struct lua_State;

namespace haylen::lua::navigation3d {

// A filter of Lua with the names of the areas of the mesh it was made for, so the costs it reports go by name.
struct ScriptedFilter {
    haylen::navigation3d::QueryFilter filter;
    std::vector<std::string> areas;
};

} // namespace haylen::lua::navigation3d

namespace haylen::lua {

template <> struct Type<lua::navigation3d::ScriptedFilter> {
    static constexpr const char* name = "haylen.QueryFilter";
    using Storage = lua::navigation3d::ScriptedFilter;
};

} // namespace haylen::lua

namespace haylen::lua::navigation3d {

// Installs the `QueryFilter` class, the costs and exclusions of the areas of one navigation mesh, which `mesh:newFilter` makes from the names of its areas.
class QueryFilterBinding final {
  public:
    static constexpr std::array<std::string_view, 2> kFields{"costs", "excluded"};

    static void install(lua_State* L);

    // Makes a filter with `mesh:newFilter({costs = {water = 4}, excluded = {'door'}})`.
    static void create(lua_State* L, const haylen::navigation3d::NavMesh& mesh, int options);

  private:
    [[nodiscard]] static std::uint8_t checkArea(lua_State* L, const ScriptedFilter& self, int index);
    static int cost(lua_State* L);
    static int excluded(lua_State* L);
};

} // namespace haylen::lua::navigation3d
