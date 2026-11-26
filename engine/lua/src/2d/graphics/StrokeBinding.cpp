#include "2d/graphics/StrokeBinding.hpp"

#include <lua.hpp>

#include <optional>
#include <string>

#include "core/FloatBufferBinding.hpp"
#include "haylen/2d/graphics/Renderer.hpp"
#include "haylen/core/Engine.hpp"
#include "haylen/lua/Runtime.hpp"
#include "haylen/lua/Stack.hpp"
#include "haylen/lua/Table.hpp"
#include "haylen/lua/TypeConverter.hpp"
#include "haylen/lua/Userdata.hpp"
#include "math/Spline2DBinding.hpp"

namespace haylen::lua::graphics2d {

void StrokeBinding::readPoints(lua_State* L, int index, std::vector<haylen::math::Vec2>& target) {
    lua::core::FloatBufferBinding::readNumbers(L, index, "points", numbers);
    if (numbers.size() % 2 != 0) {
        luaL_error(L, "The points are x and y pairs, and %d numbers leave one alone.", static_cast<int>(numbers.size()));
    }

    target.clear();
    for (std::size_t pair = 0; pair < numbers.size(); pair += 2) {
        target.push_back({numbers[pair], numbers[pair + 1]});
    }
}

// Draws with `drawStroke(points, {width, join, cap, ...})` or `drawStroke(spline, {...})`, whose option table also takes the keys of the draw order.
int StrokeBinding::drawStroke(lua_State* L) {
    const haylen::math::Spline2D* spline = lua::Userdata::test<haylen::math::Spline2D>(L, 1);
    if (spline == nullptr) {
        readPoints(L, 1, points);
    }

    haylen::graphics2d::Stroke stroke;
    std::optional<std::size_t> count;
    haylen::graphics2d::DrawOrder order;
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
                lua::core::FloatBufferBinding::readNumbers(L, lua_gettop(L), "widths", widths);
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
                lua::core::FloatBufferBinding::readNumbers(L, lua_gettop(L), "dashes", dashes);
            } else {
                lua::Table::readValue(L, key, stroke.dashOffset);
            }
        });
        // clang-format on
    }

    stroke.widths = widths;
    stroke.colors = colors;
    stroke.dashes = dashes;
    haylen::graphics2d::Renderer& renderer = lua::Runtime::getEngine(L).getRenderer2D();
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

} // namespace haylen::lua::graphics2d
