#include "2d/graphics/PixelCanvasBinding.hpp"

#include <lua.hpp>

#include "2d/graphics/Graphics2DBinding.hpp"
#include "haylen/core/Engine.hpp"
#include "haylen/lua/Binding.hpp"
#include "haylen/lua/ClassBuilder.hpp"
#include "haylen/lua/Runtime.hpp"
#include "haylen/lua/Stack.hpp"
#include "haylen/lua/Table.hpp"
#include "haylen/lua/TypeConverter.hpp"
#include "haylen/lua/Userdata.hpp"

namespace haylen::lua::graphics2d {

int PixelCanvasBinding::create(lua_State* L) {
    haylen::graphics2d::PixelCanvas::Options options;
    if (!lua_isnoneornil(L, 3)) {
        luaL_checktype(L, 3, LUA_TTABLE);
        lua::Table::checkFields(L, 3, {kOptionFields});
        lua::Table::readField(L, 3, "smooth", options.smooth);
    }

    lua::Userdata::emplace<haylen::graphics2d::PixelCanvas>(L, std::make_shared<haylen::graphics2d::PixelCanvas>(lua::Runtime::getEngine(L).getGraphics(), lua::Stack::read<int>(L, 1), lua::Stack::read<int>(L, 2), options));
    return 1;
}

// Begins with `begin(camera, canvasOptions)`, which takes the options of `graphics2d.beginTarget`.
int PixelCanvasBinding::begin(lua_State* L) {
    lua::Userdata::check<haylen::graphics2d::PixelCanvas>(L, 1).begin(lua::Runtime::getEngine(L).getRenderer2D(), lua::Userdata::check<haylen::graphics2d::Camera>(L, 2), Graphics2DBinding::readCanvasOptions(L, 3));
    return 0;
}

// Draws with `draw(x, y, width, height, order)` into the open canvas.
int PixelCanvasBinding::draw(lua_State* L) {
    const haylen::graphics2d::PixelCanvas& canvas = lua::Userdata::check<haylen::graphics2d::PixelCanvas>(L, 1);
    const haylen::math::Rect area{lua::Stack::read<float>(L, 2), lua::Stack::read<float>(L, 3), lua::Stack::read<float>(L, 4), lua::Stack::read<float>(L, 5)};
    canvas.draw(lua::Runtime::getEngine(L).getRenderer2D(), area, lua::TypeConverter::readDrawOrder(L, 6));
    return 0;
}

int PixelCanvasBinding::fraction(lua_State* L) {
    const haylen::math::Vec2 value = lua::Userdata::check<haylen::graphics2d::PixelCanvas>(L, 1).getFraction();
    lua::Stack::push(L, value.x);
    lua::Stack::push(L, value.y);
    return 2;
}

int PixelCanvasBinding::width(lua_State* L) {
    lua::Stack::push(L, static_cast<int>(lua::Userdata::check<haylen::graphics2d::PixelCanvas>(L, 1).getSize().x));
    return 1;
}

int PixelCanvasBinding::height(lua_State* L) {
    lua::Stack::push(L, static_cast<int>(lua::Userdata::check<haylen::graphics2d::PixelCanvas>(L, 1).getSize().y));
    return 1;
}

int PixelCanvasBinding::texture(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<haylen::graphics2d::PixelCanvas>(L, 1).getTarget().getTexture());
    return 1;
}

void PixelCanvasBinding::install(lua_State* L) {
    lua::ClassBuilder<haylen::graphics2d::PixelCanvas>(L).function("begin", &lua::Binding::native<&begin>).function("draw", &lua::Binding::native<&draw>).function("fraction", &lua::Binding::native<&fraction>).property("width", &width).property("height", &height).property("texture", &texture).install();
}

} // namespace haylen::lua::graphics2d
