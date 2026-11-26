#include "debug/PickRegistry.hpp"

#include <bit>
#include <cstdint>

#include "runtime/Owners.hpp"

namespace haylen::lua::debug {

void PickRegistry::remember(lua_State* L, int index, std::uint64_t owner, lua_CFunction resolver) {
    const int value = lua_absindex(L, index);
    const auto key = std::bit_cast<lua_Integer>(owner);
    lua::Owners::pushWeakTable(L, kValuesKey, "v");
    lua_pushvalue(L, value);
    lua_rawseti(L, -2, key);
    lua_pop(L, 1);

    lua::Owners::pushWeakTable(L, kResolversKey, "v");
    if (resolver != nullptr) {
        lua_pushcfunction(L, resolver);
    } else {
        lua_pushnil(L);
    }

    lua_rawseti(L, -2, key);
    lua_pop(L, 1);
}

// The selection holds the owner weakly, and the item and the resolver, which are no objects of the app, as they are.
bool PickRegistry::select(lua_State* L, const haylen::debug::Picking::Tag& tag) {
    const auto key = std::bit_cast<lua_Integer>(tag.owner);
    lua::Owners::pushWeakTable(L, kValuesKey, "v");
    lua_rawgeti(L, -1, key);
    lua::Owners::pushWeakTable(L, kResolversKey, "v");
    lua_rawgeti(L, -1, key);
    lua_remove(L, -2);
    const int resolver = lua_gettop(L);
    const int value = resolver - 1;

    clear(L);
    if (lua_isnil(L, value)) {
        lua_pop(L, 3);
        forget(L);
        return false;
    }

    lua::Owners::pushWeakTable(L, kSelectionKey, "v");
    lua_pushvalue(L, value);
    lua_setfield(L, -2, "value");
    lua_pushvalue(L, resolver);
    lua_setfield(L, -2, "resolver");
    lua_pushinteger(L, std::bit_cast<lua_Integer>(tag.item));
    lua_setfield(L, -2, "item");
    lua_pop(L, 4);
    forget(L);

    pushSelected(L);
    const bool found = !lua_isnil(L, -1);
    lua_pop(L, 1);
    if (!found) {
        clear(L);
    }

    return found;
}

void PickRegistry::clear(lua_State* L) {
    lua_pushnil(L);
    lua_setfield(L, LUA_REGISTRYINDEX, kSelectionKey);
}

// A selection without a resolver is its value, and one with a resolver is what the resolver finds for its value and its item, nothing once the value is gone.
void PickRegistry::pushSelected(lua_State* L) {
    if (lua_getfield(L, LUA_REGISTRYINDEX, kSelectionKey) != LUA_TTABLE) {
        lua_pop(L, 1);
        lua_pushnil(L);
        return;
    }

    const int selection = lua_gettop(L);
    lua_getfield(L, selection, "resolver");
    lua_getfield(L, selection, "value");
    if (lua_isnil(L, -1) || lua_isnil(L, -2)) {
        lua_replace(L, selection);
        lua_settop(L, selection);
        return;
    }

    lua_getfield(L, selection, "item");
    lua_call(L, 2, 1);
    lua_replace(L, selection);
}

std::uint64_t PickRegistry::ownerOf(const void* address) noexcept {
    return static_cast<std::uint64_t>(std::bit_cast<std::uintptr_t>(address));
}

// The values that a pick recorded serve its one search, and the next search records them again.
void PickRegistry::forget(lua_State* L) {
    lua_pushnil(L);
    lua_setfield(L, LUA_REGISTRYINDEX, kValuesKey);
    lua_pushnil(L);
    lua_setfield(L, LUA_REGISTRYINDEX, kResolversKey);
}

} // namespace haylen::lua::debug
