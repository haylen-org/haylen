#include "math/QuatBinding.hpp"

#include "haylen/lua/Binding.hpp"
#include "haylen/lua/ClassBuilder.hpp"
#include "haylen/lua/Stack.hpp"
#include "haylen/lua/TypeConverter.hpp"
#include "math/NumberText.hpp"

namespace haylen::lua::math {

// Takes nothing for the identity, the four components as numbers, or one value in any form that a `Quat` argument accepts.
int QuatBinding::newQuat(lua_State* L) {
    if (lua_isnoneornil(L, 1)) {
        lua::Stack::push(L, haylen::math::Quat::identity());
        return 1;
    }

    if (lua_type(L, 1) != LUA_TNUMBER) {
        lua::Stack::push(L, lua::Stack::read<haylen::math::Quat>(L, 1));
        return 1;
    }

    lua::Stack::push(L, haylen::math::Quat{lua::Stack::read<float>(L, 1), lua::Stack::read<float>(L, 2), lua::Stack::read<float>(L, 3), lua::Stack::read<float>(L, 4)});
    return 1;
}

haylen::math::Quat QuatBinding::fromEuler(haylen::math::Vec3 angles, std::optional<haylen::math::Quat::EulerOrder> order) {
    return haylen::math::Quat::fromEuler(angles, order.value_or(haylen::math::Quat::EulerOrder::Yxz));
}

haylen::math::Quat QuatBinding::lookRotation(haylen::math::Vec3 direction, std::optional<haylen::math::Vec3> up) {
    return haylen::math::Quat::lookRotation(direction, up.value_or(haylen::math::Vec3::up()));
}

int QuatBinding::multiply(lua_State* L) {
    lua::Stack::push(L, lua::Stack::read<haylen::math::Quat>(L, 1) * lua::Stack::read<haylen::math::Quat>(L, 2));
    return 1;
}

int QuatBinding::equal(lua_State* L) {
    lua_pushboolean(L, lua::Stack::read<haylen::math::Quat>(L, 1) == lua::Stack::read<haylen::math::Quat>(L, 2) ? 1 : 0);
    return 1;
}

int QuatBinding::toString(lua_State* L) {
    const haylen::math::Quat value = lua::Stack::read<haylen::math::Quat>(L, 1);
    lua_pushfstring(L, "Quat(%f, %f, %f, %f)", NumberText::plain(value.x), NumberText::plain(value.y), NumberText::plain(value.z), NumberText::plain(value.w));
    return 1;
}

int QuatBinding::unpack(lua_State* L) {
    const haylen::math::Quat value = lua::Stack::read<haylen::math::Quat>(L, 1);
    lua::Stack::push(L, value.x);
    lua::Stack::push(L, value.y);
    lua::Stack::push(L, value.z);
    lua::Stack::push(L, value.w);
    return 4;
}

int QuatBinding::swingTwist(lua_State* L) {
    const haylen::math::Quat::SwingTwist parts = lua::Userdata::check<haylen::math::Quat>(L, 1).getSwingTwist(lua::Stack::read<haylen::math::Vec3>(L, 2));
    lua_createtable(L, 0, 2);
    lua::Stack::push(L, parts.swing);
    lua_setfield(L, -2, "swing");
    lua::Stack::push(L, parts.twist);
    lua_setfield(L, -2, "twist");
    return 1;
}

haylen::math::Vec3 QuatBinding::rotate(const haylen::math::Quat& self, haylen::math::Vec3 vector) {
    return self.rotate(vector);
}

haylen::math::Quat QuatBinding::normalized(const haylen::math::Quat& self) {
    return self.getNormalized();
}

haylen::math::Quat QuatBinding::inverse(const haylen::math::Quat& self) {
    return self.getInverse();
}

haylen::math::Vec3 QuatBinding::axis(const haylen::math::Quat& self) {
    return self.getAxis();
}

haylen::math::Vec3 QuatBinding::toEuler(const haylen::math::Quat& self, std::optional<haylen::math::Quat::EulerOrder> order) {
    return self.toEuler(order.value_or(haylen::math::Quat::EulerOrder::Yxz));
}

bool QuatBinding::approximately(const haylen::math::Quat& self, const haylen::math::Quat& other, std::optional<float> epsilon) {
    return haylen::math::Quat::approximately(self, other, epsilon.value_or(1e-5F));
}

void QuatBinding::install(lua_State* L) {
    using haylen::math::Quat;

    lua::ClassBuilder<Quat>(L).field<&Quat::x>("x").field<&Quat::y>("y").field<&Quat::z>("z").field<&Quat::w>("w").function("rotate", &lua::Binding::function<&rotate>).method<&Quat::getLength>("length").function("normalized", &lua::Binding::function<&normalized>).function("inverse", &lua::Binding::function<&inverse>).method<&Quat::getAngle>("angle").function("axis", &lua::Binding::function<&axis>).function("toEuler", &lua::Binding::function<&toEuler>).method<&Quat::toMatrix>("matrix").function("swingTwist", &lua::Binding::native<&swingTwist>).function("dot", &lua::Binding::function<&Quat::dot>).function("angleTo", &lua::Binding::function<&Quat::angleBetween>).function("slerp", &lua::Binding::function<&Quat::slerp>).function("nlerp", &lua::Binding::function<&Quat::nlerp>).function("approximately", &lua::Binding::function<&approximately>).function("unpack", &lua::Binding::native<&unpack>).meta("__mul", &lua::Binding::native<&multiply>).meta("__eq", &lua::Binding::native<&equal>).meta("__tostring", &lua::Binding::native<&toString>).install();
}

void QuatBinding::addFunctions(lua_State* L) {
    const luaL_Reg functions[] = {
        {"quat", &lua::Binding::native<&newQuat>}, {"fromAxisAngle", &lua::Binding::function<&haylen::math::Quat::fromAxisAngle>}, {"fromEuler", &lua::Binding::function<&fromEuler>}, {"rotationBetween", &lua::Binding::function<&haylen::math::Quat::between>}, {"lookRotation", &lua::Binding::function<&lookRotation>}, {nullptr, nullptr},
    };
    luaL_setfuncs(L, functions, 0);
}

} // namespace haylen::lua::math
