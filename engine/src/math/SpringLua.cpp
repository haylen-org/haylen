#include "math/SpringLua.hpp"

#include "haylen/lua/Binding.hpp"
#include "haylen/lua/ClassBuilder.hpp"
#include "haylen/lua/Stack.hpp"
#include "haylen/lua/TypeConverter.hpp"
#include "haylen/math/Spring.hpp"

namespace haylen::lua {

template <> struct Type<math::Spring> {
    static constexpr const char* name = "haylen.Spring";
    using Storage = math::Spring;
};

} // namespace haylen::lua

namespace haylen::math {

// Creates a spring with `spring(value, smoothTime)`, both optional.
int SpringLua::newSpring(lua_State* L) {
    lua::Userdata::emplace<Spring>(L, static_cast<float>(luaL_optnumber(L, 1, 0.0)), static_cast<float>(luaL_optnumber(L, 2, 0.2)));
    return 1;
}

int SpringLua::update(lua_State* L) {
    Spring& spring = lua::Userdata::check<Spring>(L, 1);
    lua::Stack::push(L, spring.update(lua::Stack::read<float>(L, 2), lua::Stack::read<float>(L, 3)));
    return 1;
}

// Returns the next value and velocity with `smoothDamp(current, target, velocity, smoothTime, dt)`, for numbers or vectors.
int SpringLua::smoothDamp(lua_State* L) {
    const auto smoothTime = lua::Stack::read<float>(L, 4);
    const auto deltaSeconds = lua::Stack::read<float>(L, 5);
    if (lua_type(L, 1) == LUA_TNUMBER) {
        auto velocity = lua::Stack::read<float>(L, 3);
        lua::Stack::push(L, Spring::smoothDamp(lua::Stack::read<float>(L, 1), lua::Stack::read<float>(L, 2), velocity, smoothTime, deltaSeconds));
        lua::Stack::push(L, velocity);
        return 2;
    }
    auto velocity = lua::Stack::read<Vec2>(L, 3);
    lua::Stack::push(L, Spring::smoothDamp(lua::Stack::read<Vec2>(L, 1), lua::Stack::read<Vec2>(L, 2), velocity, smoothTime, deltaSeconds));
    lua::Stack::push(L, velocity);
    return 2;
}

void SpringLua::install(lua_State* L) {
    lua::ClassBuilder<Spring>(L).function("update", &lua::Binding::native<&update>).accessor<&Spring::getValue, &Spring::setValue>("value").accessor<&Spring::getVelocity, &Spring::setVelocity>("velocity").accessor<&Spring::getSmoothTime, &Spring::setSmoothTime>("smoothTime").install();
}

void SpringLua::addFunctions(lua_State* L) {
    lua_pushcfunction(L, &lua::Binding::native<&newSpring>);
    lua_setfield(L, -2, "spring");
    lua_pushcfunction(L, &lua::Binding::native<&smoothDamp>);
    lua_setfield(L, -2, "smoothDamp");
}

} // namespace haylen::math
