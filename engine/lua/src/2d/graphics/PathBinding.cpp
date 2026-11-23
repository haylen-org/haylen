#include "2d/graphics/PathBinding.hpp"

#include <lua.hpp>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

#include "2d/graphics/StrokeBinding.hpp"
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

namespace haylen::lua::graphics2d {

int PathBinding::create(lua_State* L) {
    lua::Userdata::emplace<haylen::graphics2d::Path>(L, std::make_shared<haylen::graphics2d::Path>());
    return 1;
}

haylen::math::Vec2 PathBinding::checkPoint(lua_State* L, int index) {
    return {lua::Stack::read<float>(L, index), lua::Stack::read<float>(L, index + 1)};
}

int PathBinding::moveTo(lua_State* L) {
    lua::Userdata::check<haylen::graphics2d::Path>(L, 1).moveTo(checkPoint(L, 2));
    lua_settop(L, 1);
    return 1;
}

int PathBinding::lineTo(lua_State* L) {
    lua::Userdata::check<haylen::graphics2d::Path>(L, 1).lineTo(checkPoint(L, 2));
    lua_settop(L, 1);
    return 1;
}

int PathBinding::quadraticTo(lua_State* L) {
    lua::Userdata::check<haylen::graphics2d::Path>(L, 1).quadraticTo(checkPoint(L, 2), checkPoint(L, 4));
    lua_settop(L, 1);
    return 1;
}

int PathBinding::cubicTo(lua_State* L) {
    lua::Userdata::check<haylen::graphics2d::Path>(L, 1).cubicTo(checkPoint(L, 2), checkPoint(L, 4), checkPoint(L, 6));
    lua_settop(L, 1);
    return 1;
}

// Adds an arc with `arc(x, y, radius, startAngle, sweep)`, the angles in radians.
int PathBinding::arc(lua_State* L) {
    lua::Userdata::check<haylen::graphics2d::Path>(L, 1).arc(checkPoint(L, 2), lua::Stack::read<float>(L, 4), lua::Stack::read<float>(L, 5), lua::Stack::read<float>(L, 6));
    lua_settop(L, 1);
    return 1;
}

int PathBinding::close(lua_State* L) {
    lua::Userdata::check<haylen::graphics2d::Path>(L, 1).close();
    lua_settop(L, 1);
    return 1;
}

int PathBinding::clear(lua_State* L) {
    lua::Userdata::check<haylen::graphics2d::Path>(L, 1).clear();
    lua_settop(L, 1);
    return 1;
}

// Fills with `fill(x, y, {color, fillRule, texture, rotation, scaleX, scaleY, ...})`, whose option table also takes the keys of the draw order.
int PathBinding::fill(lua_State* L) {
    const haylen::graphics2d::Path& path = lua::Userdata::check<haylen::graphics2d::Path>(L, 1);
    const haylen::math::Vec2 position{lua::Converter<float>::readOr(L, 2, 0.0), lua::Converter<float>::readOr(L, 3, 0.0)};
    haylen::graphics2d::Path::Fill style;
    float rotation = 0.0F;
    haylen::math::Vec2 scale{1.0F, 1.0F};
    haylen::graphics2d::DrawOrder order;
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

    lua::Runtime::getEngine(L).getRenderer2D().fillPath(path, style, order, haylen::math::Transform2D::compose(position, rotation, scale));
    return 0;
}

// Strokes with `stroke(x, y, {width, color, join, cap, dashes, rotation, scaleX, scaleY, ...})`, whose option table also takes the keys of the draw order.
int PathBinding::stroke(lua_State* L) {
    const haylen::graphics2d::Path& path = lua::Userdata::check<haylen::graphics2d::Path>(L, 1);
    const haylen::math::Vec2 position{lua::Converter<float>::readOr(L, 2, 0.0), lua::Converter<float>::readOr(L, 3, 0.0)};
    haylen::graphics2d::Stroke style;
    float rotation = 0.0F;
    haylen::math::Vec2 scale{1.0F, 1.0F};
    haylen::graphics2d::DrawOrder order;
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
                StrokeBinding::readNumbers(L, lua_gettop(L), "dashes", dashes);
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
    lua::Runtime::getEngine(L).getRenderer2D().strokePath(path, style, order, haylen::math::Transform2D::compose(position, rotation, scale));
    return 0;
}

// Returns the outlines with `outlines(tolerance)` as a shape of the polygon functions of `haylen.math`, a list of outlines of points, and a list that tells for each outline whether it closes.
int PathBinding::outlines(lua_State* L) {
    const haylen::graphics2d::Path& path = lua::Userdata::check<haylen::graphics2d::Path>(L, 1);
    haylen::graphics2d::Path::Outlines flattened;
    path.flatten(lua::Converter<float>::readOr(L, 2, 0.25), flattened);

    std::vector<std::vector<haylen::math::Vec2>> shape;
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

int PathBinding::isEmpty(lua_State* L) {
    lua_pushboolean(L, lua::Userdata::check<haylen::graphics2d::Path>(L, 1).empty() ? 1 : 0);
    return 1;
}

void PathBinding::install(lua_State* L) {
    lua::ClassBuilder<haylen::graphics2d::Path>(L).function("moveTo", &lua::Binding::native<&moveTo>).function("lineTo", &lua::Binding::native<&lineTo>).function("quadraticTo", &lua::Binding::native<&quadraticTo>).function("cubicTo", &lua::Binding::native<&cubicTo>).function("arc", &lua::Binding::native<&arc>).function("close", &lua::Binding::native<&close>).function("clear", &lua::Binding::native<&clear>).function("fill", &lua::Binding::native<&fill>).function("stroke", &lua::Binding::native<&stroke>).function("outlines", &lua::Binding::native<&outlines>).property("empty", &isEmpty).install();
}

} // namespace haylen::lua::graphics2d
