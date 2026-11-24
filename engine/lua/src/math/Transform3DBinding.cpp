#include "math/Transform3DBinding.hpp"

#include <vector>

#include "haylen/lua/Binding.hpp"
#include "haylen/lua/ClassBuilder.hpp"
#include "haylen/lua/Stack.hpp"
#include "haylen/lua/TypeConverter.hpp"

namespace haylen::lua::math {

// Takes nothing for the identity, a `Transform3D` or a `Mat4` to copy or split, or the position, the rotation and the scale in order, each optional.
int Transform3DBinding::newTransform(lua_State* L) {
    if (const haylen::math::Transform3D* copied = lua::Userdata::test<haylen::math::Transform3D>(L, 1)) {
        lua::Stack::push(L, *copied);
        return 1;
    }

    if (const haylen::math::Mat4* matrix = lua::Userdata::test<haylen::math::Mat4>(L, 1)) {
        lua::Stack::push(L, haylen::math::Transform3D::fromMatrix(*matrix));
        return 1;
    }

    haylen::math::Transform3D transform;
    if (!lua_isnoneornil(L, 1)) {
        transform.position = lua::Stack::read<haylen::math::Vec3>(L, 1);
    }

    if (!lua_isnoneornil(L, 2)) {
        transform.rotation = lua::Stack::read<haylen::math::Quat>(L, 2);
    }

    if (!lua_isnoneornil(L, 3)) {
        transform.scale = lua::Stack::read<haylen::math::Vec3>(L, 3);
    }

    lua::Stack::push(L, transform);
    return 1;
}

haylen::math::DualQuat Transform3DBinding::newDualQuat(std::optional<haylen::math::Quat> rotation, std::optional<haylen::math::Vec3> translation) {
    return haylen::math::DualQuat::fromRotationTranslation(rotation.value_or(haylen::math::Quat::identity()), translation.value_or(haylen::math::Vec3{}));
}

int Transform3DBinding::blendDualQuats(lua_State* L) {
    const std::vector<haylen::math::DualQuat> parts = lua::Stack::read<std::vector<haylen::math::DualQuat>>(L, 1);
    const std::vector<float> weights = lua::Stack::read<std::vector<float>>(L, 2);
    lua::Stack::push(L, haylen::math::DualQuat::blend(parts, weights));
    return 1;
}

int Transform3DBinding::transformMultiply(lua_State* L) {
    lua::Stack::push(L, lua::Stack::read<haylen::math::Transform3D>(L, 1) * lua::Stack::read<haylen::math::Transform3D>(L, 2));
    return 1;
}

int Transform3DBinding::transformEqual(lua_State* L) {
    lua_pushboolean(L, lua::Stack::read<haylen::math::Transform3D>(L, 1) == lua::Stack::read<haylen::math::Transform3D>(L, 2) ? 1 : 0);
    return 1;
}

int Transform3DBinding::transformToString(lua_State* L) {
    const haylen::math::Transform3D& t = lua::Userdata::check<haylen::math::Transform3D>(L, 1);
    lua_pushfstring(L, "Transform3D((%f, %f, %f), ", static_cast<double>(t.position.x), static_cast<double>(t.position.y), static_cast<double>(t.position.z));
    lua_pushfstring(L, "(%f, %f, %f, %f), ", static_cast<double>(t.rotation.x), static_cast<double>(t.rotation.y), static_cast<double>(t.rotation.z), static_cast<double>(t.rotation.w));
    lua_pushfstring(L, "(%f, %f, %f))", static_cast<double>(t.scale.x), static_cast<double>(t.scale.y), static_cast<double>(t.scale.z));
    lua_concat(L, 3);
    return 1;
}

haylen::math::Vec3 Transform3DBinding::applyNormal(const haylen::math::Transform3D& self, haylen::math::Vec3 normal) {
    return self.applyNormal(normal);
}

haylen::math::Vec3 Transform3DBinding::applyInverse(const haylen::math::Transform3D& self, haylen::math::Vec3 point) {
    return self.applyInverse(point);
}

haylen::math::Vec3 Transform3DBinding::applyInverseVector(const haylen::math::Transform3D& self, haylen::math::Vec3 vector) {
    return self.applyInverseVector(vector);
}

haylen::math::Transform3D Transform3DBinding::inverse(const haylen::math::Transform3D& self) {
    return self.getInverse();
}

bool Transform3DBinding::transformApproximately(const haylen::math::Transform3D& self, const haylen::math::Transform3D& other, std::optional<float> epsilon) {
    return haylen::math::Transform3D::approximately(self, other, epsilon.value_or(1e-5F));
}

int Transform3DBinding::dualQuatMultiply(lua_State* L) {
    lua::Stack::push(L, lua::Stack::read<haylen::math::DualQuat>(L, 1) * lua::Stack::read<haylen::math::DualQuat>(L, 2));
    return 1;
}

int Transform3DBinding::dualQuatToString(lua_State* L) {
    const haylen::math::DualQuat& value = lua::Userdata::check<haylen::math::DualQuat>(L, 1);
    const haylen::math::Vec3 translation = value.getTranslation();
    lua_pushfstring(L, "DualQuat((%f, %f, %f, %f), ", static_cast<double>(value.real.x), static_cast<double>(value.real.y), static_cast<double>(value.real.z), static_cast<double>(value.real.w));
    lua_pushfstring(L, "(%f, %f, %f))", static_cast<double>(translation.x), static_cast<double>(translation.y), static_cast<double>(translation.z));
    lua_concat(L, 2);
    return 1;
}

haylen::math::DualQuat Transform3DBinding::dualQuatNormalized(const haylen::math::DualQuat& self) {
    return self.getNormalized();
}

bool Transform3DBinding::dualQuatApproximately(const haylen::math::DualQuat& self, const haylen::math::DualQuat& other, std::optional<float> epsilon) {
    return haylen::math::DualQuat::approximately(self, other, epsilon.value_or(1e-5F));
}

void Transform3DBinding::install(lua_State* L) {
    using haylen::math::DualQuat;
    using haylen::math::Transform3D;
    using haylen::math::Vec3;

    lua::ClassBuilder<Transform3D>(L).field<&Transform3D::position>("position").field<&Transform3D::rotation>("rotation").field<&Transform3D::scale>("scale").nestedField<&Transform3D::position, &Vec3::x>("x").nestedField<&Transform3D::position, &Vec3::y>("y").nestedField<&Transform3D::position, &Vec3::z>("z").method<&Transform3D::apply>("apply").method<&Transform3D::applyVector>("applyVector").function("applyNormal", &lua::Binding::function<&applyNormal>).function("applyInverse", &lua::Binding::function<&applyInverse>).function("applyInverseVector", &lua::Binding::function<&applyInverseVector>).function("inverse", &lua::Binding::function<&inverse>).method<&Transform3D::toMatrix>("matrix").function("lerp", &lua::Binding::function<&Transform3D::lerp>).function("approximately", &lua::Binding::function<&transformApproximately>).meta("__mul", &lua::Binding::native<&transformMultiply>).meta("__eq", &lua::Binding::native<&transformEqual>).meta("__tostring", &lua::Binding::native<&transformToString>).install();

    lua::ClassBuilder<DualQuat>(L).method<&DualQuat::getRotation>("rotation").method<&DualQuat::getTranslation>("translation").method<&DualQuat::apply>("apply").function("normalized", &lua::Binding::function<&dualQuatNormalized>).method<&DualQuat::toMatrix>("matrix").function("approximately", &lua::Binding::function<&dualQuatApproximately>).meta("__mul", &lua::Binding::native<&dualQuatMultiply>).meta("__tostring", &lua::Binding::native<&dualQuatToString>).install();
}

void Transform3DBinding::addFunctions(lua_State* L) {
    const luaL_Reg functions[] = {
        {"transform3d", &lua::Binding::native<&newTransform>},
        {"dualQuat", &lua::Binding::function<&newDualQuat>},
        {"blendDualQuats", &lua::Binding::native<&blendDualQuats>},
        {nullptr, nullptr},
    };
    luaL_setfuncs(L, functions, 0);
}

} // namespace haylen::lua::math
