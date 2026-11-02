#include "2d/graphics/PathLua.hpp"

#include <lua.hpp>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

#include "2d/graphics/StrokeLua.hpp"
#include "haylen/2d/graphics/Renderer.hpp"
#include "haylen/core/Engine.hpp"
#include "haylen/lua/Binding.hpp"
#include "haylen/lua/ClassBuilder.hpp"
#include "haylen/lua/Runtime.hpp"
#include "haylen/lua/Stack.hpp"
#include "haylen/lua/Table.hpp"
#include "haylen/lua/TypeConverter.hpp"
#include "haylen/lua/Userdata.hpp"
#include "haylen/math/Transform2D.hpp"

namespace haylen::graphics2d {

int PathLua::create(lua_State* L) {
    lua::Userdata::emplace<Path>(L, std::make_shared<Path>());
    return 1;
}

math::Vec2 PathLua::checkPoint(lua_State* L, int index) {
    return {static_cast<float>(luaL_checknumber(L, index)), static_cast<float>(luaL_checknumber(L, index + 1))};
}

int PathLua::moveTo(lua_State* L) {
    lua::Userdata::check<Path>(L, 1).moveTo(checkPoint(L, 2));
    lua_settop(L, 1);
    return 1;
}

int PathLua::lineTo(lua_State* L) {
    lua::Userdata::check<Path>(L, 1).lineTo(checkPoint(L, 2));
    lua_settop(L, 1);
    return 1;
}

int PathLua::quadraticTo(lua_State* L) {
    lua::Userdata::check<Path>(L, 1).quadraticTo(checkPoint(L, 2), checkPoint(L, 4));
    lua_settop(L, 1);
    return 1;
}

int PathLua::cubicTo(lua_State* L) {
    lua::Userdata::check<Path>(L, 1).cubicTo(checkPoint(L, 2), checkPoint(L, 4), checkPoint(L, 6));
    lua_settop(L, 1);
    return 1;
}

// Adds an arc with `arc(x, y, radius, startAngle, sweep)`, the angles in radians.
int PathLua::arc(lua_State* L) {
    lua::Userdata::check<Path>(L, 1).arc(checkPoint(L, 2), static_cast<float>(luaL_checknumber(L, 4)), static_cast<float>(luaL_checknumber(L, 5)), static_cast<float>(luaL_checknumber(L, 6)));
    lua_settop(L, 1);
    return 1;
}

int PathLua::close(lua_State* L) {
    lua::Userdata::check<Path>(L, 1).close();
    lua_settop(L, 1);
    return 1;
}

int PathLua::clear(lua_State* L) {
    lua::Userdata::check<Path>(L, 1).clear();
    lua_settop(L, 1);
    return 1;
}

// Fills with `fill(x, y, {color, fillRule, texture, rotation, scaleX, scaleY, ...})`, whose option table also takes the keys of the draw order.
int PathLua::fill(lua_State* L) {
    const Path& path = lua::Userdata::check<Path>(L, 1);
    const math::Vec2 position{static_cast<float>(luaL_optnumber(L, 2, 0.0)), static_cast<float>(luaL_optnumber(L, 3, 0.0))};
    Path::Fill style;
    float rotation = 0.0F;
    math::Vec2 scale{1.0F, 1.0F};
    DrawOrder order;
    if (!lua_isnoneornil(L, 4)) {
        order = lua::TypeConverter::readDrawOrder(L, 4, {kFillFields});
        // clang-format off
        lua::Table::readFields(L, 4, kFillFields, {lua::TypeConverter::kDrawOrderFields}, [L, &style, &rotation, &scale](std::string_view key) {
            if (key == "color") {
                lua::Table::readValue(L, key, style.color);
            } else if (key == "fillRule") {
                lua::Table::readValue(L, key, style.rule);
            } else if (key == "texture") {
                lua::Table::readValue(L, key, style.texture);
            } else if (key == "rotation") {
                lua::Table::readValue(L, key, rotation);
            } else if (key == "scaleX") {
                lua::Table::readValue(L, key, scale.x);
            } else {
                lua::Table::readValue(L, key, scale.y);
            }
        });
        // clang-format on
    }
    lua::Runtime::getEngine(L).getRenderer2D().fillPath(path, style, order, math::Transform2D::compose(position, rotation, scale));
    return 0;
}

// Strokes with `stroke(x, y, {width, color, join, cap, dashes, rotation, scaleX, scaleY, ...})`, whose option table also takes the keys of the draw order.
int PathLua::stroke(lua_State* L) {
    const Path& path = lua::Userdata::check<Path>(L, 1);
    const math::Vec2 position{static_cast<float>(luaL_optnumber(L, 2, 0.0)), static_cast<float>(luaL_optnumber(L, 3, 0.0))};
    Stroke style;
    float rotation = 0.0F;
    math::Vec2 scale{1.0F, 1.0F};
    DrawOrder order;
    dashes.clear();
    if (!lua_isnoneornil(L, 4)) {
        order = lua::TypeConverter::readDrawOrder(L, 4, {kStrokeFields});
        // clang-format off
        lua::Table::readFields(L, 4, kStrokeFields, {lua::TypeConverter::kDrawOrderFields}, [L, &style, &rotation, &scale](std::string_view key) {
            if (key == "width") {
                lua::Table::readValue(L, key, style.width);
            } else if (key == "color") {
                lua::Table::readValue(L, key, style.color);
            } else if (key == "join") {
                lua::Table::readValue(L, key, style.join);
            } else if (key == "cap") {
                lua::Table::readValue(L, key, style.cap);
            } else if (key == "miterLimit") {
                lua::Table::readValue(L, key, style.miterLimit);
            } else if (key == "texture") {
                lua::Table::readValue(L, key, style.texture);
            } else if (key == "textureMode") {
                lua::Table::readValue(L, key, style.textureMode);
            } else if (key == "textureLength") {
                lua::Table::readValue(L, key, style.textureLength);
            } else if (key == "dashes") {
                StrokeLua::readNumbers(L, lua_gettop(L), "dashes", dashes);
            } else if (key == "dashOffset") {
                lua::Table::readValue(L, key, style.dashOffset);
            } else if (key == "rotation") {
                lua::Table::readValue(L, key, rotation);
            } else if (key == "scaleX") {
                lua::Table::readValue(L, key, scale.x);
            } else {
                lua::Table::readValue(L, key, scale.y);
            }
        });
        // clang-format on
    }
    style.dashes = dashes;
    lua::Runtime::getEngine(L).getRenderer2D().strokePath(path, style, order, math::Transform2D::compose(position, rotation, scale));
    return 0;
}

// Returns the outlines with `outlines(tolerance)` as a shape of the polygon functions of `haylen.math`, a list of outlines of points, and a list that tells for each outline whether it closes.
int PathLua::outlines(lua_State* L) {
    const Path& path = lua::Userdata::check<Path>(L, 1);
    Path::Outlines flattened;
    path.flatten(static_cast<float>(luaL_optnumber(L, 2, 0.25)), flattened);

    std::vector<std::vector<math::Vec2>> shape;
    shape.reserve(flattened.counts.size());
    std::size_t first = 0;
    for (const std::uint32_t count : flattened.counts) {
        shape.emplace_back(flattened.points.begin() + static_cast<std::ptrdiff_t>(first), flattened.points.begin() + static_cast<std::ptrdiff_t>(first + count));
        first += count;
    }
    lua::Stack::push(L, shape);
    lua_createtable(L, static_cast<int>(flattened.closed.size()), 0);
    for (std::size_t index = 0; index < flattened.closed.size(); ++index) {
        lua_pushboolean(L, flattened.closed[index] != 0 ? 1 : 0);
        lua_rawseti(L, -2, static_cast<lua_Integer>(index + 1));
    }
    return 2;
}

int PathLua::isEmpty(lua_State* L) {
    lua_pushboolean(L, lua::Userdata::check<Path>(L, 1).empty() ? 1 : 0);
    return 1;
}

void PathLua::install(lua_State* L) {
    lua::ClassBuilder<Path>(L).function("moveTo", &lua::Binding::native<&moveTo>).function("lineTo", &lua::Binding::native<&lineTo>).function("quadraticTo", &lua::Binding::native<&quadraticTo>).function("cubicTo", &lua::Binding::native<&cubicTo>).function("arc", &lua::Binding::native<&arc>).function("close", &lua::Binding::native<&close>).function("clear", &lua::Binding::native<&clear>).function("fill", &lua::Binding::native<&fill>).function("stroke", &lua::Binding::native<&stroke>).function("outlines", &lua::Binding::native<&outlines>).property("empty", &isEmpty).install();
}

} // namespace haylen::graphics2d
