#include "3d/navigation/QueryFilterBinding.hpp"

#include <lua.hpp>

#include <algorithm>
#include <stdexcept>
#include <utility>

#include "haylen/lua/Binding.hpp"
#include "haylen/lua/ClassBuilder.hpp"
#include "haylen/lua/Stack.hpp"
#include "haylen/lua/Table.hpp"
#include "haylen/lua/Userdata.hpp"

namespace haylen::lua::navigation3d {

std::uint8_t QueryFilterBinding::checkArea(lua_State* L, const ScriptedFilter& self, int index) {
    const auto name = lua::Stack::read<std::string_view>(L, index);
    const auto found = std::ranges::find(self.areas, name);
    if (found == self.areas.end()) {
        throw std::invalid_argument("The navigation mesh of the filter has no area \"" + std::string(name) + "\".");
    }

    return static_cast<std::uint8_t>(found - self.areas.begin() + 1);
}

void QueryFilterBinding::create(lua_State* L, const haylen::navigation3d::NavMesh& mesh, int options) {
    ScriptedFilter made{.filter = {}, .areas = mesh.getAreas()};
    if (!lua_isnoneornil(L, options)) {
        luaL_checktype(L, options, LUA_TTABLE);
        lua::Table::checkFields(L, options, {kFields});

        // Costs map the names of areas to numbers, and exclusions list names.
        if (lua_getfield(L, options, "costs") != LUA_TNIL) {
            luaL_checktype(L, -1, LUA_TTABLE);
            lua_pushnil(L);
            while (lua_next(L, -2) != 0) {
                const std::uint8_t area = mesh.getArea(lua::Stack::read<std::string_view>(L, -2));
                made.filter.setCost(area, lua::Stack::read<float>(L, -1));
                lua_pop(L, 1);
            }
        }
        lua_pop(L, 1);

        if (lua_getfield(L, options, "excluded") != LUA_TNIL) {
            for (const std::string& name : lua::Stack::read<std::vector<std::string>>(L, -1)) {
                made.filter.setExcluded(mesh.getArea(name), true);
            }
        }
        lua_pop(L, 1);
    }

    lua::Userdata::emplace<ScriptedFilter>(L, std::move(made));
}

// Returns the cost of an area with `filter:cost(area)`.
int QueryFilterBinding::cost(lua_State* L) {
    const ScriptedFilter& self = lua::Userdata::check<ScriptedFilter>(L, 1);
    lua::Stack::push(L, self.filter.getCost(checkArea(L, self, 2)));
    return 1;
}

// Tells whether a query skips an area with `filter:excluded(area)`.
int QueryFilterBinding::excluded(lua_State* L) {
    const ScriptedFilter& self = lua::Userdata::check<ScriptedFilter>(L, 1);
    lua::Stack::push(L, self.filter.isExcluded(checkArea(L, self, 2)));
    return 1;
}

void QueryFilterBinding::install(lua_State* L) {
    lua::ClassBuilder<ScriptedFilter>(L).function("cost", &lua::Binding::native<&cost>).function("excluded", &lua::Binding::native<&excluded>).install();
}

} // namespace haylen::lua::navigation3d
