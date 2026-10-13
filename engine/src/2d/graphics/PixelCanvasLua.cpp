#include "2d/graphics/PixelCanvasLua.hpp"

#include <lua.hpp>

#include "2d/graphics/Graphics2DLua.hpp"
#include "haylen/core/Engine.hpp"
#include "haylen/lua/Binding.hpp"
#include "haylen/lua/ClassBuilder.hpp"
#include "haylen/lua/Runtime.hpp"
#include "haylen/lua/Stack.hpp"
#include "haylen/lua/Table.hpp"
#include "haylen/lua/TypeConverter.hpp"
#include "haylen/lua/Userdata.hpp"

namespace haylen::graphics2d {

int PixelCanvasLua::create(lua_State* L) {
    PixelCanvas::Options options;
    if (!lua_isnoneornil(L, 3)) {
        luaL_checktype(L, 3, LUA_TTABLE);
        lua::Table::checkFields(L, 3, {kOptionFields});
        lua::Table::readField(L, 3, "smooth", options.smooth);
    }
    lua::Userdata::emplace<PixelCanvas>(L, std::make_shared<PixelCanvas>(lua::Runtime::getEngine(L).getGraphics(), static_cast<int>(luaL_checkinteger(L, 1)), static_cast<int>(luaL_checkinteger(L, 2)), options));
    return 1;
}

// Begins with `begin(camera, canvasOptions)`, which takes the options of `graphics2d.beginTarget`.
int PixelCanvasLua::begin(lua_State* L) {
    lua::Userdata::check<PixelCanvas>(L, 1).begin(lua::Runtime::getEngine(L).getRenderer2D(), lua::Userdata::check<Camera>(L, 2), Graphics2DLua::readCanvasOptions(L, 3));
    return 0;
}

// Draws with `draw(x, y, width, height, order)` into the open canvas.
int PixelCanvasLua::draw(lua_State* L) {
    const PixelCanvas& canvas = lua::Userdata::check<PixelCanvas>(L, 1);
    const math::Rect area{static_cast<float>(luaL_checknumber(L, 2)), static_cast<float>(luaL_checknumber(L, 3)), static_cast<float>(luaL_checknumber(L, 4)), static_cast<float>(luaL_checknumber(L, 5))};
    canvas.draw(lua::Runtime::getEngine(L).getRenderer2D(), area, lua::TypeConverter::readDrawOrder(L, 6));
    return 0;
}

int PixelCanvasLua::fraction(lua_State* L) {
    const math::Vec2 value = lua::Userdata::check<PixelCanvas>(L, 1).getFraction();
    lua::Stack::push(L, value.x);
    lua::Stack::push(L, value.y);
    return 2;
}

int PixelCanvasLua::width(lua_State* L) {
    lua::Stack::push(L, static_cast<int>(lua::Userdata::check<PixelCanvas>(L, 1).getSize().x));
    return 1;
}

int PixelCanvasLua::height(lua_State* L) {
    lua::Stack::push(L, static_cast<int>(lua::Userdata::check<PixelCanvas>(L, 1).getSize().y));
    return 1;
}

int PixelCanvasLua::texture(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<PixelCanvas>(L, 1).getTarget().getTexture());
    return 1;
}

void PixelCanvasLua::install(lua_State* L) {
    lua::ClassBuilder<PixelCanvas>(L).function("begin", &lua::Binding::native<&begin>).function("draw", &lua::Binding::native<&draw>).function("fraction", &lua::Binding::native<&fraction>).property("width", &width).property("height", &height).property("texture", &texture).install();
}

} // namespace haylen::graphics2d
