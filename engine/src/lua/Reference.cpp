#include "haylen/lua/Reference.hpp"

#include <utility>

#include "haylen/lua/Runtime.hpp"

namespace haylen::lua {

Reference::Reference(lua_State* L, int index) : state(Runtime::getMainThread(L)) {
    lua_pushvalue(L, index);
    reference = luaL_ref(L, LUA_REGISTRYINDEX);
}

Reference::~Reference() {
    reset();
}

Reference::Reference(Reference&& other) noexcept : state(std::exchange(other.state, nullptr)), reference(std::exchange(other.reference, LUA_NOREF)) {}

Reference& Reference::operator=(Reference&& other) noexcept {
    if (this != &other) {
        reset();
        state = std::exchange(other.state, nullptr);
        reference = std::exchange(other.reference, LUA_NOREF);
    }
    return *this;
}

void Reference::push(lua_State* L) const {
    lua_rawgeti(L, LUA_REGISTRYINDEX, reference);
}

void Reference::reset() noexcept {
    if (state != nullptr && reference != LUA_NOREF) {
        luaL_unref(state, LUA_REGISTRYINDEX, reference);
    }
    reference = LUA_NOREF;
}

} // namespace haylen::lua
