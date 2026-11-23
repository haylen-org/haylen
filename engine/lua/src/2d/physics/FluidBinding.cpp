#include "2d/physics/FluidBinding.hpp"

#include <lua.hpp>

#include <span>

#include "2d/physics/Physics2DBinding.hpp"
#include "2d/physics/ScriptedOwner.hpp"
#include "2d/physics/ShapeBinding.hpp"
#include "core/FloatBufferBinding.hpp"
#include "haylen/2d/graphics/Renderer.hpp"
#include "haylen/2d/physics/Fluid.hpp"
#include "haylen/core/Engine.hpp"
#include "haylen/lua/Binding.hpp"
#include "haylen/lua/ClassBuilder.hpp"
#include "haylen/lua/Runtime.hpp"
#include "haylen/lua/Stack.hpp"
#include "haylen/lua/Table.hpp"
#include "haylen/lua/TypeConverter.hpp"

namespace haylen::lua {

template <> struct Type<lua::physics2d::ScriptedOwner<haylen::physics2d::Fluid>> {
    static constexpr const char* name = "haylen.Fluid";
    using Storage = lua::physics2d::ScriptedOwner<haylen::physics2d::Fluid>;
};

} // namespace haylen::lua

namespace haylen::lua::physics2d {

// Creates a fluid with `newFluid(world, {radius, smoothingRadius, density, friction, restitution, viscosity, iterations, gravityScale, maxSpeed, maxParticles, category, mask, group})`. Its user value is the world object, which steps it.
int FluidBinding::newFluid(lua_State* L) {
    const std::shared_ptr<haylen::physics2d::World>& world = lua::Userdata::checkShared<haylen::physics2d::World>(L, 1);
    haylen::physics2d::Fluid::Options options;
    if (!lua_isnoneornil(L, 2)) {
        luaL_checktype(L, 2, LUA_TTABLE);
        lua::Table::checkFields(L, 2, {kFluidFields});
        lua::Table::readField(L, 2, "radius", options.radius);
        lua::Table::readField(L, 2, "smoothingRadius", options.smoothingRadius);
        lua::Table::readField(L, 2, "density", options.density);
        lua::Table::readField(L, 2, "friction", options.friction);
        lua::Table::readField(L, 2, "restitution", options.restitution);
        lua::Table::readField(L, 2, "viscosity", options.viscosity);
        lua::Table::readField(L, 2, "iterations", options.iterations);
        lua::Table::readField(L, 2, "gravityScale", options.gravityScale);
        lua::Table::readField(L, 2, "maxSpeed", options.maxSpeed);
        lua::Table::readField(L, 2, "maxParticles", options.maxParticles);
        options.filter = ShapeBinding::readFilter(L, 2, {});
    }

    lua::Userdata::emplace<ScriptedOwner<haylen::physics2d::Fluid>>(L, world, options);
    lua_pushvalue(L, 1);
    lua_setiuservalue(L, -2, 1);
    return 1;
}

int FluidBinding::spawn(lua_State* L) {
    haylen::physics2d::Fluid& fluid = lua::Userdata::check<ScriptedOwner<haylen::physics2d::Fluid>>(L, 1).object;
    const haylen::math::Vec2 position{lua::Stack::read<float>(L, 2), lua::Stack::read<float>(L, 3)};
    const haylen::math::Vec2 velocity{lua::Converter<float>::readOr(L, 4, 0.0), lua::Converter<float>::readOr(L, 5, 0.0)};
    lua::Stack::push(L, fluid.spawn(position, velocity));
    return 1;
}

int FluidBinding::fill(lua_State* L) {
    haylen::physics2d::Fluid& fluid = lua::Userdata::check<ScriptedOwner<haylen::physics2d::Fluid>>(L, 1).object;
    const auto area = lua::Stack::read<haylen::math::Rect>(L, 2);
    const haylen::math::Vec2 velocity{lua::Converter<float>::readOr(L, 3, 0.0), lua::Converter<float>::readOr(L, 4, 0.0)};
    lua::Stack::push(L, fluid.fill(area, velocity));
    return 1;
}

int FluidBinding::remove(lua_State* L) {
    const auto particle = lua::Stack::read<std::size_t>(L, 2);
    luaL_argcheck(L, particle >= 1, 2, "particles count from 1");
    lua::Userdata::check<ScriptedOwner<haylen::physics2d::Fluid>>(L, 1).object.remove(particle - 1);
    return 0;
}

int FluidBinding::clear(lua_State* L) {
    lua::Userdata::check<ScriptedOwner<haylen::physics2d::Fluid>>(L, 1).object.clear();
    return 0;
}

// Reusing the buffer or the list from frame to frame keeps the bulk reads from allocating.
int FluidBinding::pushPairs(lua_State* L, bool velocities) {
    const haylen::physics2d::Fluid& fluid = lua::Userdata::check<ScriptedOwner<haylen::physics2d::Fluid>>(L, 1).object;
    const std::span<const haylen::math::Vec2> values = velocities ? fluid.getVelocities() : fluid.getPositions();
    if (haylen::core::FloatBuffer* buffer = lua::Userdata::test<haylen::core::FloatBuffer>(L, 2)) {
        const lua_Integer first = luaL_optinteger(L, 3, 1);
        const std::span<float> floats = buffer->getValues();
        luaL_argcheck(L, first >= 1 && static_cast<std::size_t>(first - 1) + values.size() * 2 <= floats.size(), 2, "the buffer needs two values for each particle");

        for (std::size_t index = 0; index < values.size(); ++index) {
            floats[static_cast<std::size_t>(first - 1) + index * 2] = values[index].x;
            floats[static_cast<std::size_t>(first - 1) + index * 2 + 1] = values[index].y;
        }

        lua_pushvalue(L, 2);
        return 1;
    }

    if (lua_isnoneornil(L, 2)) {
        lua_createtable(L, static_cast<int>(values.size() * 2), 0);
    } else {
        luaL_checktype(L, 2, LUA_TTABLE);
        lua_pushvalue(L, 2);
    }

    for (std::size_t index = 0; index < values.size(); ++index) {
        lua_pushnumber(L, values[index].x);
        lua_rawseti(L, -2, static_cast<lua_Integer>(index * 2 + 1));
        lua_pushnumber(L, values[index].y);
        lua_rawseti(L, -2, static_cast<lua_Integer>(index * 2 + 2));
    }

    for (auto extra = static_cast<lua_Integer>(values.size() * 2 + 1); lua_rawgeti(L, -1, extra) != LUA_TNIL; ++extra) {
        lua_pop(L, 1);
        lua_pushnil(L);
        lua_rawseti(L, -2, extra);
    }

    lua_pop(L, 1);
    return 1;
}

int FluidBinding::positions(lua_State* L) {
    return pushPairs(L, false);
}

int FluidBinding::velocities(lua_State* L) {
    return pushPairs(L, true);
}

// Draws the particles as metaballs with `draw({radius, color, outlineColor, outlineWidth, threshold, layer, depth, ...})`, straight from the arrays of the fluid. The radius of the balls defaults to 0.6 smoothing radii.
int FluidBinding::draw(lua_State* L) {
    const haylen::physics2d::Fluid& fluid = lua::Userdata::check<ScriptedOwner<haylen::physics2d::Fluid>>(L, 1).object;
    haylen::graphics2d::Renderer::MetaballStyle style;
    float ball = fluid.getOptions().smoothingRadius * kBallRadius;
    if (!lua_isnoneornil(L, 2)) {
        luaL_checktype(L, 2, LUA_TTABLE);
        lua::Table::checkFields(L, 2, {kDrawFields, lua::TypeConverter::kDrawOrderFields});
        lua::Table::readField(L, 2, "radius", ball);
        lua::Table::readField(L, 2, "color", style.color);
        lua::Table::readField(L, 2, "outlineColor", style.outlineColor);
        lua::Table::readField(L, 2, "outlineWidth", style.outlineWidth);
        lua::Table::readField(L, 2, "threshold", style.threshold);
    }

    if (fluid.size() > 0) {
        lua::Runtime::getEngine(L).getRenderer2D().drawMetaballs(fluid.getPositions(), ball, style, lua::TypeConverter::readDrawOrder(L, 2, {kDrawFields}));
    }

    return 0;
}

int FluidBinding::size(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<ScriptedOwner<haylen::physics2d::Fluid>>(L, 1).object.size());
    return 1;
}

int FluidBinding::radius(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<ScriptedOwner<haylen::physics2d::Fluid>>(L, 1).object.getOptions().radius);
    return 1;
}

int FluidBinding::stepMilliseconds(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<ScriptedOwner<haylen::physics2d::Fluid>>(L, 1).object.getStepMilliseconds());
    return 1;
}

void FluidBinding::install(lua_State* L) {
    lua::ClassBuilder<ScriptedOwner<haylen::physics2d::Fluid>>(L).function("spawn", &lua::Binding::native<&spawn>).function("fill", &lua::Binding::native<&fill>).function("remove", &lua::Binding::native<&remove>).function("clear", &lua::Binding::native<&clear>).function("positions", &lua::Binding::native<&positions>).function("velocities", &lua::Binding::native<&velocities>).function("draw", &lua::Binding::native<&draw>).property("size", &size).property("radius", &radius).property("stepMilliseconds", &stepMilliseconds).install();
}

void FluidBinding::addFunctions(lua_State* L) {
    lua_pushcfunction(L, &lua::Binding::native<&newFluid>);
    lua_setfield(L, -2, "newFluid");
}

} // namespace haylen::lua::physics2d
