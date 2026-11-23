#include "2d/animation/ChainBinding.hpp"

#include <lua.hpp>

#include <cstdint>
#include <vector>

#include "core/FloatBufferBinding.hpp"
#include "haylen/lua/Binding.hpp"
#include "haylen/lua/ClassBuilder.hpp"
#include "haylen/lua/Stack.hpp"
#include "haylen/lua/Table.hpp"
#include "haylen/lua/TypeConverter.hpp"
#include "haylen/lua/Userdata.hpp"

namespace haylen::lua::animation2d {

// Takes the lengths of the segments as `lengths`, or as `segments` of the same `length`.
int ChainBinding::create(lua_State* L) {
    luaL_checktype(L, 1, LUA_TTABLE);
    lua::Table::checkFields(L, 1, {kOptionFields});
    haylen::animation2d::Chain::Options options;
    lua::Table::readField(L, 1, "x", options.root.x);
    lua::Table::readField(L, 1, "y", options.root.y);
    lua::Table::readField(L, 1, "lengths", options.lengths);
    lua::Table::readField(L, 1, "angle", options.angle);
    lua::Table::readField(L, 1, "maxBend", options.maxBend);

    lua_Integer segments = 0;
    float length = 0.0F;
    lua::Table::readField(L, 1, "segments", segments);
    lua::Table::readField(L, 1, "length", length);
    const bool even = segments != 0 || length != 0.0F;
    if (even == !options.lengths.empty()) {
        return luaL_error(L, "A chain takes the lengths of its segments either as \"lengths\" or as \"segments\" of the same \"length\".");
    }

    if (even) {
        if (segments < 1 || static_cast<std::uint64_t>(segments) > haylen::animation2d::Chain::kMaxSegments) {
            return luaL_error(L, "A chain needs from 1 to %d segments in \"segments\".", static_cast<int>(haylen::animation2d::Chain::kMaxSegments));
        }

        options.lengths.assign(static_cast<std::size_t>(segments), length);
    }

    lua::Userdata::emplace<haylen::animation2d::Chain>(L, std::make_shared<haylen::animation2d::Chain>(options));
    return 1;
}

std::size_t ChainBinding::checkPoint(lua_State* L, int index, const haylen::animation2d::Chain& chain) {
    const lua_Integer number = luaL_checkinteger(L, index);
    luaL_argcheck(L, number >= 1 && static_cast<std::size_t>(number) <= chain.size(), index, "points are numbered from one to the count of the chain");
    return static_cast<std::size_t>(number - 1);
}

// Reads the options of a reach in one pass, since a leg reaches every frame.
int ChainBinding::reach(lua_State* L) {
    haylen::animation2d::Chain& chain = lua::Userdata::check<haylen::animation2d::Chain>(L, 1);
    const haylen::math::Vec2 target{lua::Stack::read<float>(L, 2), lua::Stack::read<float>(L, 3)};
    haylen::animation2d::Chain::Reach options;
    if (!lua_isnoneornil(L, 4)) {
        luaL_checktype(L, 4, LUA_TTABLE);
        // clang-format off
        lua::Table::readFields(L, 4, kReachFields, {}, [L, &options](std::string_view key) {
            if (key == "pole") {
                haylen::math::Vec2 pole;
                lua::Table::readValue(L, key, pole);
                options.pole = pole;
            } else if (key == "iterations") {
                lua::Table::readValue(L, key, options.iterations);
            } else {
                lua::Table::readValue(L, key, options.tolerance);
            }
        });
        // clang-format on
        luaL_argcheck(L, options.iterations >= 1 && options.iterations <= haylen::animation2d::Chain::kMaxIterations, 4, "a reach needs from 1 to 1000 iterations");
    }

    lua::Stack::push(L, chain.reach(target, options));
    return 1;
}

int ChainBinding::follow(lua_State* L) {
    lua::Userdata::check<haylen::animation2d::Chain>(L, 1).follow({lua::Stack::read<float>(L, 2), lua::Stack::read<float>(L, 3)});
    return 0;
}

int ChainBinding::straighten(lua_State* L) {
    lua::Userdata::check<haylen::animation2d::Chain>(L, 1).straighten(lua::Stack::read<float>(L, 2));
    return 0;
}

int ChainBinding::point(lua_State* L) {
    const haylen::animation2d::Chain& chain = lua::Userdata::check<haylen::animation2d::Chain>(L, 1);
    const haylen::math::Vec2 value = chain.getPoints()[checkPoint(L, 2, chain)];
    lua::Stack::push(L, value.x);
    lua::Stack::push(L, value.y);
    return 2;
}

int ChainBinding::setPoint(lua_State* L) {
    haylen::animation2d::Chain& chain = lua::Userdata::check<haylen::animation2d::Chain>(L, 1);
    chain.setPoint(checkPoint(L, 2, chain), {lua::Stack::read<float>(L, 3), lua::Stack::read<float>(L, 4)});
    return 0;
}

// Returns the angle of a segment, which runs from its point to the next one.
int ChainBinding::angle(lua_State* L) {
    const haylen::animation2d::Chain& chain = lua::Userdata::check<haylen::animation2d::Chain>(L, 1);
    const lua_Integer segment = luaL_checkinteger(L, 2);
    luaL_argcheck(L, segment >= 1 && static_cast<std::size_t>(segment) < chain.size(), 2, "segments are numbered from one to the count of the chain minus one");
    const std::span<const haylen::math::Vec2> points = chain.getPoints();
    lua::Stack::push(L, (points[static_cast<std::size_t>(segment)] - points[static_cast<std::size_t>(segment - 1)]).getAngle());
    return 1;
}

// Writes the x and y of every point into a float buffer from the value at `first` on.
int ChainBinding::writePoints(lua_State* L) {
    const haylen::animation2d::Chain& chain = lua::Userdata::check<haylen::animation2d::Chain>(L, 1);
    haylen::core::FloatBuffer& buffer = lua::Userdata::check<haylen::core::FloatBuffer>(L, 2);
    const lua_Integer first = luaL_optinteger(L, 3, 1);
    luaL_argcheck(L, first >= 1, 3, "the values of a buffer are numbered from one");
    const std::size_t start = static_cast<std::size_t>(first - 1);
    if (start + chain.size() * 2 > buffer.size()) {
        return luaL_error(L, "The buffer holds %d values, and the %d points of the chain need %d from value %d on.", static_cast<int>(buffer.size()), static_cast<int>(chain.size()), static_cast<int>(chain.size() * 2), static_cast<int>(first));
    }

    const std::span<float> values = buffer.getValues().subspan(start);
    for (std::size_t index = 0; index < chain.size(); ++index) {
        values[index * 2] = chain.getPoints()[index].x;
        values[index * 2 + 1] = chain.getPoints()[index].y;
    }

    return 0;
}

int ChainBinding::getX(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<haylen::animation2d::Chain>(L, 1).getRoot().x);
    return 1;
}

int ChainBinding::setX(lua_State* L) {
    haylen::animation2d::Chain& chain = lua::Userdata::check<haylen::animation2d::Chain>(L, 1);
    chain.setRoot({lua::Stack::read<float>(L, 3), chain.getRoot().y});
    return 0;
}

int ChainBinding::getY(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<haylen::animation2d::Chain>(L, 1).getRoot().y);
    return 1;
}

int ChainBinding::setY(lua_State* L) {
    haylen::animation2d::Chain& chain = lua::Userdata::check<haylen::animation2d::Chain>(L, 1);
    chain.setRoot({chain.getRoot().x, lua::Stack::read<float>(L, 3)});
    return 0;
}

int ChainBinding::count(lua_State* L) {
    lua::Stack::push(L, static_cast<lua_Integer>(lua::Userdata::check<haylen::animation2d::Chain>(L, 1).size()));
    return 1;
}

int ChainBinding::length(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<haylen::animation2d::Chain>(L, 1).getTotalLength());
    return 1;
}

void ChainBinding::install(lua_State* L) {
    lua::ClassBuilder<haylen::animation2d::Chain>(L).function("reach", &lua::Binding::native<&reach>).function("follow", &follow).function("straighten", &straighten).function("point", &point).function("setPoint", &setPoint).function("angle", &angle).function("writePoints", &writePoints).property("x", &getX, &setX).property("y", &getY, &setY).property("count", &count).property("length", &length).accessor<&haylen::animation2d::Chain::getMaxBend, &haylen::animation2d::Chain::setMaxBend>("maxBend").install();
}

} // namespace haylen::lua::animation2d
