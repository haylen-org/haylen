#include "2d/graphics/GroupLua.hpp"

#include <lua.hpp>

#include "haylen/core/Engine.hpp"
#include "haylen/lua/Binding.hpp"
#include "haylen/lua/Runtime.hpp"
#include "haylen/lua/Table.hpp"
#include "haylen/lua/TypeConverter.hpp"

namespace haylen::graphics2d {

// Begins with `beginGroup({opacity, layer, ...})`, whose table also takes the keys of the draw order of the image.
int GroupLua::beginGroup(lua_State* L) {
    Renderer::GroupOptions options;
    if (!lua_isnoneornil(L, 1)) {
        options.order = lua::TypeConverter::readDrawOrder(L, 1, {kGroupFields});
        lua::Table::readField(L, 1, "opacity", options.opacity);
    }
    lua::Runtime::getEngine(L).getRenderer2D().beginGroup(options);
    return 0;
}

int GroupLua::endGroup(lua_State* L) {
    lua::Runtime::getEngine(L).getRenderer2D().endGroup();
    return 0;
}

int GroupLua::beginMask(lua_State* L) {
    Renderer::MaskMode mode = Renderer::MaskMode::Inside;
    DrawOrder order;
    if (!lua_isnoneornil(L, 1)) {
        order = lua::TypeConverter::readDrawOrder(L, 1, {kMaskFields});
        lua::Table::readField(L, 1, "mode", mode);
    }
    lua::Runtime::getEngine(L).getRenderer2D().pushMask(mode, order);
    return 0;
}

int GroupLua::endMaskShape(lua_State* L) {
    lua::Runtime::getEngine(L).getRenderer2D().endMaskShape();
    return 0;
}

int GroupLua::pushMask(lua_State* L) {
    luaL_checktype(L, 1, LUA_TFUNCTION);
    lua_settop(L, 2);
    lua_pushcfunction(L, &lua::Binding::native<&beginMask>);
    lua_pushvalue(L, 2);
    lua_call(L, 1, 0);
    lua_pushvalue(L, 1);
    lua_call(L, 0, 0);
    lua_pushcfunction(L, &lua::Binding::native<&endMaskShape>);
    lua_call(L, 0, 0);
    return 0;
}

int GroupLua::popMask(lua_State* L) {
    lua::Runtime::getEngine(L).getRenderer2D().popMask();
    return 0;
}

} // namespace haylen::graphics2d
