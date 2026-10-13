#include "2d/graphics/StrokeLua.hpp"

#include <lua.hpp>

#include <string>

#include "core/FloatBufferLua.hpp"
#include "haylen/2d/graphics/Renderer.hpp"
#include "haylen/core/Engine.hpp"
#include "haylen/lua/Runtime.hpp"
#include "haylen/lua/Stack.hpp"
#include "haylen/lua/Table.hpp"
#include "haylen/lua/TypeConverter.hpp"
#include "haylen/lua/Userdata.hpp"

namespace haylen::graphics2d {

void StrokeLua::readNumbers(lua_State* L, int index, std::string_view what, std::vector<float>& values) {
    values.clear();
    if (const core::FloatBuffer* buffer = lua::Userdata::test<core::FloatBuffer>(L, index)) {
        values.assign(buffer->getValues().begin(), buffer->getValues().end());
        return;
    }
    if (!lua_istable(L, index)) {
        luaL_error(L, "The %s are a list of numbers or a float buffer.", std::string(what).c_str());
        return;
    }
    const lua_Integer count = luaL_len(L, index);
    for (lua_Integer place = 1; place <= count; ++place) {
        if (lua_rawgeti(L, index, place) != LUA_TNUMBER) {
            luaL_error(L, "The %s are a list of numbers, and entry %d is not a number.", std::string(what).c_str(), static_cast<int>(place));
        }
        values.push_back(static_cast<float>(lua_tonumber(L, -1)));
        lua_pop(L, 1);
    }
}

void StrokeLua::readPoints(lua_State* L, int index, std::vector<math::Vec2>& target) {
    readNumbers(L, index, "points", numbers);
    if (numbers.size() % 2 != 0) {
        luaL_error(L, "The points are x and y pairs, and %d numbers leave one alone.", static_cast<int>(numbers.size()));
    }
    target.clear();
    for (std::size_t pair = 0; pair < numbers.size(); pair += 2) {
        target.push_back({numbers[pair], numbers[pair + 1]});
    }
}

// Draws with `drawStroke(points, {width, join, cap, ...})`, whose option table also takes the keys of the draw order.
int StrokeLua::drawStroke(lua_State* L) {
    readPoints(L, 1, points);
    Stroke stroke;
    std::size_t count = points.size();
    DrawOrder order;
    widths.clear();
    colors.clear();
    dashes.clear();
    if (!lua_isnoneornil(L, 2)) {
        order = lua::TypeConverter::readDrawOrder(L, 2, {kFields});
        // clang-format off
        lua::Table::readFields(L, 2, kFields, {lua::TypeConverter::kDrawOrderFields}, [L, &stroke, &count](std::string_view key) {
            if (key == "count") {
                lua::Table::readValue(L, key, count);
            } else if (key == "width") {
                lua::Table::readValue(L, key, stroke.width);
            } else if (key == "widths") {
                readNumbers(L, lua_gettop(L), "widths", widths);
            } else if (key == "color") {
                lua::Table::readValue(L, key, stroke.color);
            } else if (key == "colors") {
                lua::Table::readValue(L, key, colors);
            } else if (key == "join") {
                lua::Table::readValue(L, key, stroke.join);
            } else if (key == "cap") {
                lua::Table::readValue(L, key, stroke.cap);
            } else if (key == "miterLimit") {
                lua::Table::readValue(L, key, stroke.miterLimit);
            } else if (key == "closed") {
                lua::Table::readValue(L, key, stroke.closed);
            } else if (key == "texture") {
                lua::Table::readValue(L, key, stroke.texture);
            } else if (key == "textureMode") {
                lua::Table::readValue(L, key, stroke.textureMode);
            } else if (key == "textureLength") {
                lua::Table::readValue(L, key, stroke.textureLength);
            } else if (key == "dashes") {
                readNumbers(L, lua_gettop(L), "dashes", dashes);
            } else {
                lua::Table::readValue(L, key, stroke.dashOffset);
            }
        });
        // clang-format on
    }
    if (count > points.size()) {
        return luaL_error(L, "The stroke draws %d points of the %d it was given.", static_cast<int>(count), static_cast<int>(points.size()));
    }
    if (count < 2) {
        return luaL_error(L, "A stroke needs at least two points.");
    }
    stroke.widths = widths;
    stroke.colors = colors;
    stroke.dashes = dashes;
    lua::Runtime::getEngine(L).getRenderer2D().drawStroke(std::span(points).first(count), stroke, order);
    return 0;
}

} // namespace haylen::graphics2d
