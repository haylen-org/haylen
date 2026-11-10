#include "2d/graphics/GroupLua.hpp"

#include <lua.hpp>

#include <new>

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

// The error of a shape function goes on with its whole stack, and the closer ends the mask while the error unwinds.
int GroupLua::pushMask(lua_State* L) {
    luaL_checktype(L, 1, LUA_TFUNCTION);
    lua_settop(L, 2);
    MaskCloser& closer = pushCloser(L);
    lua_toclose(L, 3);

    lua_pushcfunction(L, &lua::Binding::native<&beginMask>);
    lua_pushvalue(L, 2);
    lua_call(L, 1, 0);
    closer.shapeOpen = true;

    lua_pushvalue(L, 1);
    lua_call(L, 0, 0);
    closer.shapeOpen = false;

    lua_pushcfunction(L, &lua::Binding::native<&endMaskShape>);
    lua_call(L, 0, 0);
    return 0;
}

// Pushes a closer that no running `pushMask` holds, which the registry keeps, so masks allocate nothing once their nesting has been reached.
GroupLua::MaskCloser& GroupLua::pushCloser(lua_State* L) {
    if (lua_getfield(L, LUA_REGISTRYINDEX, kClosersKey) != LUA_TTABLE) {
        lua_pop(L, 1);
        lua_newtable(L);
        lua_createtable(L, 0, 1);
        lua_pushcfunction(L, &lua::Binding::native<&closeMask>);
        lua_setfield(L, -2, "__close");
        lua_setfield(L, -2, "metatable");
        lua_pushvalue(L, -1);
        lua_setfield(L, LUA_REGISTRYINDEX, kClosersKey);
    }
    const int closers = lua_gettop(L);

    for (lua_Integer level = 1;; ++level) {
        if (lua_rawgeti(L, closers, level) == LUA_TNIL) {
            lua_pop(L, 1);
            new (lua_newuserdatauv(L, sizeof(MaskCloser), 0)) MaskCloser{};
            lua_getfield(L, closers, "metatable");
            lua_setmetatable(L, -2);
            lua_pushvalue(L, -1);
            lua_rawseti(L, closers, level);
        }

        auto& closer = *static_cast<MaskCloser*>(lua_touserdata(L, -1));
        if (!closer.busy) {
            closer.busy = true;
            lua_remove(L, closers);
            return closer;
        }
        lua_pop(L, 1);
    }
}

// Frees the closer once its `pushMask` returns or its error unwinds, and in the second case ends the mask of the shape function that failed, whose content stays empty.
int GroupLua::closeMask(lua_State* L) {
    auto& closer = *static_cast<MaskCloser*>(lua_touserdata(L, 1));
    closer.busy = false;
    if (!closer.shapeOpen) {
        return 0;
    }

    closer.shapeOpen = false;
    Renderer& renderer = lua::Runtime::getEngine(L).getRenderer2D();
    renderer.endMaskShape();
    renderer.popMask();
    return 0;
}

int GroupLua::popMask(lua_State* L) {
    lua::Runtime::getEngine(L).getRenderer2D().popMask();
    return 0;
}

} // namespace haylen::graphics2d
