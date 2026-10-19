#include "2d/graphics/StrokeLua.hpp"

#include <lua.hpp>

#include <optional>
#include <string>

#include "core/FloatBufferLua.hpp"
#include "haylen/2d/graphics/Renderer.hpp"
#include "haylen/core/Engine.hpp"
#include "haylen/lua/Runtime.hpp"
#include "haylen/lua/Stack.hpp"
#include "haylen/lua/Table.hpp"
#include "haylen/lua/TypeConverter.hpp"
#include "haylen/lua/Userdata.hpp"
#include "math/SplineLua.hpp"

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

// Draws with `drawStroke(points, {width, join, cap, ...})` or `drawStroke(spline, {...})`, whose option table also takes the keys of the draw order.
int StrokeLua::drawStroke(lua_State* L) {
    const math::Spline* spline = lua::Userdata::test<math::Spline>(L, 1);
    if (spline == nullptr) {
        readPoints(L, 1, points);
    }
    Stroke stroke;
    std::optional<std::size_t> count;
    DrawOrder order;
    widths.clear();
    colors.clear();
    dashes.clear();
    if (!lua_isnoneornil(L, 2)) {
        order = lua::TypeConverter::readDrawOrder(L, 2, {kFields});
        // clang-format off
        lua::Table::readFields(L, 2, kFields, {lua::TypeConverter::kDrawOrderFields}, [L, &stroke, &count](std::string_view key) {
            if (key == "count") {
                lua::Table::readValue(L, key, count.emplace());
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
    stroke.widths = widths;
    stroke.colors = colors;
    stroke.dashes = dashes;
    Renderer& renderer = lua::Runtime::getEngine(L).getRenderer2D();
    if (spline != nullptr) {
        if (count) {
            return luaL_error(L, "A stroke along a spline draws the whole curve, without \"count\".");
        }
        renderer.drawStroke(*spline, stroke, order);
        return 0;
    }

    const std::size_t drawn = count.value_or(points.size());
    if (drawn > points.size()) {
        return luaL_error(L, "The stroke draws %d points of the %d it was given.", static_cast<int>(drawn), static_cast<int>(points.size()));
    }
    if (drawn < 2) {
        return luaL_error(L, "A stroke needs at least two points.");
    }
    renderer.drawStroke(std::span(points).first(drawn), stroke, order);
    return 0;
}

} // namespace haylen::graphics2d
