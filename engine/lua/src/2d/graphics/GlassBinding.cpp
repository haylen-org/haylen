#include "2d/graphics/GlassBinding.hpp"

#include <lua.hpp>

#include "2d/graphics/Graphics2DBinding.hpp"
#include "haylen/2d/graphics/Glass.hpp"
#include "haylen/2d/graphics/Shape.hpp"
#include "haylen/core/Engine.hpp"
#include "haylen/lua/Runtime.hpp"
#include "haylen/lua/Stack.hpp"
#include "haylen/lua/Table.hpp"
#include "haylen/lua/TypeConverter.hpp"

namespace haylen::lua::graphics2d {

int GlassBinding::drawGlass(lua_State* L) {
    haylen::graphics2d::Shape shape{.bounds = lua::Stack::read<haylen::math::Rect>(L, 1)};
    haylen::graphics2d::Glass glass;
    if (!lua_isnoneornil(L, 2)) {
        luaL_checktype(L, 2, LUA_TTABLE);
        // clang-format off
        lua::Table::readFields(L, 2, kGlassFields, {lua::TypeConverter::kDrawOrderFields}, [L, &shape, &glass](std::string_view key) {
            readGlassField(L, key, glass);
            Graphics2DBinding::readShapeField(L, key, shape);
        });
        // clang-format on
    }

    lua::Runtime::getEngine(L).getRenderer2D().drawGlass(shape, glass, lua::TypeConverter::readDrawOrder(L, 2, {kGlassFields}));
    return 0;
}

void GlassBinding::readGlassField(lua_State* L, std::string_view key, haylen::graphics2d::Glass& glass) {
    if (key == "blur") {
        lua::Table::readValue(L, key, glass.blur);
    } else if (key == "saturation") {
        lua::Table::readValue(L, key, glass.saturation);
    } else if (key == "brightness") {
        lua::Table::readValue(L, key, glass.brightness);
    } else if (key == "tint") {
        lua::Table::readValue(L, key, glass.tint);
    } else if (key == "rim") {
        lua::Table::readValue(L, key, glass.rim);
    } else if (key == "rimWidth") {
        lua::Table::readValue(L, key, glass.rimWidth);
    } else if (key == "refraction") {
        lua::Table::readValue(L, key, glass.refraction);
    }
}

} // namespace haylen::lua::graphics2d
