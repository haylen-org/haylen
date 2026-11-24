#include "math/VectorBinding.hpp"

#include "haylen/lua/Binding.hpp"
#include "haylen/lua/ClassBuilder.hpp"
#include "math/NumberText.hpp"

namespace haylen::lua::math {

// Takes the three components as numbers, or one value in any form that a `Vec3` argument accepts.
int VectorBinding::newVec3(lua_State* L) {
    if (!lua_isnoneornil(L, 1) && lua_type(L, 1) != LUA_TNUMBER) {
        lua::Stack::push(L, lua::Stack::read<haylen::math::Vec3>(L, 1));
        return 1;
    }

    lua::Stack::push(L, haylen::math::Vec3{lua::Converter<float>::readOr(L, 1, 0.0), lua::Converter<float>::readOr(L, 2, 0.0), lua::Converter<float>::readOr(L, 3, 0.0)});
    return 1;
}

// Takes the four components as numbers, one value in any form that a `Vec4` argument accepts, or a `Vec3` with `w` after it.
int VectorBinding::newVec4(lua_State* L) {
    if (!lua_isnoneornil(L, 1) && lua_type(L, 1) != LUA_TNUMBER) {
        if (lua_isnoneornil(L, 2)) {
            lua::Stack::push(L, lua::Stack::read<haylen::math::Vec4>(L, 1));
            return 1;
        }

        lua::Stack::push(L, haylen::math::Vec4::fromVec3(lua::Stack::read<haylen::math::Vec3>(L, 1), lua::Stack::read<float>(L, 2)));
        return 1;
    }

    lua::Stack::push(L, haylen::math::Vec4{lua::Converter<float>::readOr(L, 1, 0.0), lua::Converter<float>::readOr(L, 2, 0.0), lua::Converter<float>::readOr(L, 3, 0.0), lua::Converter<float>::readOr(L, 4, 0.0)});
    return 1;
}

int VectorBinding::vec3ToString(lua_State* L) {
    const haylen::math::Vec3 value = lua::Stack::read<haylen::math::Vec3>(L, 1);
    lua_pushfstring(L, "Vec3(%f, %f, %f)", NumberText::plain(value.x), NumberText::plain(value.y), NumberText::plain(value.z));
    return 1;
}

int VectorBinding::vec4ToString(lua_State* L) {
    const haylen::math::Vec4 value = lua::Stack::read<haylen::math::Vec4>(L, 1);
    lua_pushfstring(L, "Vec4(%f, %f, %f, %f)", NumberText::plain(value.x), NumberText::plain(value.y), NumberText::plain(value.z), NumberText::plain(value.w));
    return 1;
}

int VectorBinding::vec3Unpack(lua_State* L) {
    const haylen::math::Vec3 value = lua::Stack::read<haylen::math::Vec3>(L, 1);
    lua::Stack::push(L, value.x);
    lua::Stack::push(L, value.y);
    lua::Stack::push(L, value.z);
    return 3;
}

int VectorBinding::vec4Unpack(lua_State* L) {
    const haylen::math::Vec4 value = lua::Stack::read<haylen::math::Vec4>(L, 1);
    lua::Stack::push(L, value.x);
    lua::Stack::push(L, value.y);
    lua::Stack::push(L, value.z);
    lua::Stack::push(L, value.w);
    return 4;
}

void VectorBinding::install(lua_State* L) {
    using haylen::math::Vec3;
    using haylen::math::Vec4;

    lua::ClassBuilder<Vec3>(L).field<&Vec3::x>("x").field<&Vec3::y>("y").field<&Vec3::z>("z").method<&Vec3::getLength>("length").method<&Vec3::getLengthSquared>("lengthSquared").method<&Vec3::isZero>("isZero").method<&Vec3::isFinite>("isFinite").function("normalized", &lua::Binding::function<&normalized<Vec3>>).function("dot", &lua::Binding::function<&Vec3::dot>).function("cross", &lua::Binding::function<&Vec3::cross>).function("distance", &lua::Binding::function<&Vec3::distance>).function("distanceSquared", &lua::Binding::function<&Vec3::distanceSquared>).function("project", &lua::Binding::function<&Vec3::project>).function("projectOnPlane", &lua::Binding::function<&Vec3::projectOnPlane>).function("reflect", &lua::Binding::function<&Vec3::reflect>).function("refract", &lua::Binding::function<&Vec3::refract>).function("lerp", &lua::Binding::function<&Vec3::lerp>).function("min", &lua::Binding::function<&Vec3::min>).function("max", &lua::Binding::function<&Vec3::max>).function("approximately", &lua::Binding::function<&approximately<Vec3>>).function("unpack", &vec3Unpack).meta("__add", &add<Vec3>).meta("__sub", &subtract<Vec3>).meta("__mul", &multiply<Vec3>).meta("__div", &divide<Vec3>).meta("__unm", &negate<Vec3>).meta("__eq", &equal<Vec3>).meta("__tostring", &vec3ToString).install();

    lua::ClassBuilder<Vec4>(L).field<&Vec4::x>("x").field<&Vec4::y>("y").field<&Vec4::z>("z").field<&Vec4::w>("w").method<&Vec4::getXyz>("xyz").method<&Vec4::getLength>("length").method<&Vec4::getLengthSquared>("lengthSquared").method<&Vec4::isZero>("isZero").method<&Vec4::isFinite>("isFinite").function("normalized", &lua::Binding::function<&normalized<Vec4>>).function("dot", &lua::Binding::function<&Vec4::dot>).function("distance", &lua::Binding::function<&Vec4::distance>).function("distanceSquared", &lua::Binding::function<&Vec4::distanceSquared>).function("lerp", &lua::Binding::function<&Vec4::lerp>).function("min", &lua::Binding::function<&Vec4::min>).function("max", &lua::Binding::function<&Vec4::max>).function("approximately", &lua::Binding::function<&approximately<Vec4>>).function("unpack", &vec4Unpack).meta("__add", &add<Vec4>).meta("__sub", &subtract<Vec4>).meta("__mul", &multiply<Vec4>).meta("__div", &divide<Vec4>).meta("__unm", &negate<Vec4>).meta("__eq", &equal<Vec4>).meta("__tostring", &vec4ToString).install();
}

void VectorBinding::addFunctions(lua_State* L) {
    const luaL_Reg functions[] = {
        {"vec3", &lua::Binding::native<&newVec3>}, {"vec4", &lua::Binding::native<&newVec4>}, {"right", &lua::Binding::function<&haylen::math::Vec3::right>}, {"up", &lua::Binding::function<&haylen::math::Vec3::up>}, {"forward", &lua::Binding::function<&haylen::math::Vec3::forward>}, {nullptr, nullptr},
    };
    luaL_setfuncs(L, functions, 0);
}

} // namespace haylen::lua::math
