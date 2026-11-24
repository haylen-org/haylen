#include "math/Geometry3Binding.hpp"

#include <cmath>
#include <stdexcept>

#include "haylen/lua/Binding.hpp"
#include "haylen/lua/ClassBuilder.hpp"
#include "haylen/lua/Stack.hpp"
#include "haylen/lua/TypeConverter.hpp"

namespace haylen::lua::math {

int Geometry3Binding::planeToString(lua_State* L) {
    const haylen::math::Plane& plane = lua::Userdata::check<haylen::math::Plane>(L, 1);
    lua_pushfstring(L, "Plane((%f, %f, %f), %f)", static_cast<double>(plane.normal.x), static_cast<double>(plane.normal.y), static_cast<double>(plane.normal.z), static_cast<double>(plane.distance));
    return 1;
}

int Geometry3Binding::planeEqual(lua_State* L) {
    lua_pushboolean(L, lua::Stack::read<haylen::math::Plane>(L, 1) == lua::Stack::read<haylen::math::Plane>(L, 2) ? 1 : 0);
    return 1;
}

// A plane keeps a normal of length one, which every distance it measures relies on.
void Geometry3Binding::checkNormal(haylen::math::Vec3 normal) {
    if (!(std::fabs(normal.getLength() - 1.0F) <= 1e-4F)) {
        throw std::invalid_argument("The normal of a plane must have a length of one. Make the plane with \"plane\" of \"haylen.math\", which scales its normal.");
    }
}

void Geometry3Binding::install(lua_State* L) {
    using haylen::math::Plane;

    lua::ClassBuilder<Plane>(L).checkedField<&Plane::normal, &checkNormal>("normal").field<&Plane::distance>("distance").method<&Plane::getSignedDistance>("signedDistance").method<&Plane::getClosestPoint>("closestPoint").method<&Plane::getCoefficients>("coefficients").meta("__eq", &lua::Binding::native<&planeEqual>).meta("__tostring", &lua::Binding::native<&planeToString>).install();
}

void Geometry3Binding::addFunctions(lua_State* L) {
    const luaL_Reg functions[] = {
        {"plane", &lua::Binding::function<&haylen::math::Plane::fromNormal>},
        {"planeFromPoint", &lua::Binding::function<&haylen::math::Plane::fromPoint>},
        {"planeFromPoints", &lua::Binding::function<&haylen::math::Plane::fromPoints>},
        {nullptr, nullptr},
    };
    luaL_setfuncs(L, functions, 0);
}

} // namespace haylen::lua::math
