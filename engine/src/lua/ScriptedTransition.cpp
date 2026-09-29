#include "lua/ScriptedTransition.hpp"

#include <lua.hpp>

#include <stdexcept>

#include "haylen/lua/Runtime.hpp"
#include "haylen/lua/Stack.hpp"
#include "haylen/lua/Table.hpp"
#include "haylen/lua/TypeConverter.hpp"

namespace haylen::lua {

// An effect that covers the screen at its switchProgress exits there, and only an effect that shows both scenes, with a switchProgress of 0, chooses where the leaving scenes exit, at the end by default.
ScriptedTransition::ScriptedTransition(lua_State* L, int index) : table(L, index) {
    const int effect = lua_absindex(L, index);
    Table::readField(L, effect, "switchProgress", switchProgress);
    if (switchProgress < 0.0F || switchProgress > 1.0F) {
        throw std::invalid_argument("A transition effect needs a switchProgress between 0 and 1.");
    }
    lua_getfield(L, effect, "exitProgress");
    const bool exitGiven = !lua_isnil(L, -1);
    lua_pop(L, 1);
    if (exitGiven && switchProgress > 0.0F) {
        throw std::invalid_argument("A transition effect that covers the screen at its switchProgress takes no exitProgress.");
    }
    exitProgress = switchProgress > 0.0F ? switchProgress : 1.0F;
    Table::readField(L, effect, "exitProgress", exitProgress);
    if (exitProgress < 0.0F || exitProgress > 1.0F) {
        throw std::invalid_argument("A transition effect needs an exitProgress between 0 and 1.");
    }
    const bool drawable = lua_getfield(L, effect, "render") == LUA_TFUNCTION;
    lua_pop(L, 1);
    if (!drawable) {
        throw std::invalid_argument("A transition effect needs a render method.");
    }
}

// The effect draws through haylen.graphics2d, which reaches the same renderer.
void ScriptedTransition::render(graphics2d::Renderer&, const Frames& frames, float progress) {
    // clang-format off
    Runtime::protectedRun(table.getState(), [this, &frames, progress](lua_State* L) {
        table.push(L);
        lua_getfield(L, -1, "render");
        lua_insert(L, -2);
        lua_pushnumber(L, progress);
        Stack::push(L, frames.outgoing);
        Stack::push(L, frames.incoming);
        lua_call(L, 4, 0);
    });
    // clang-format on
}

} // namespace haylen::lua
