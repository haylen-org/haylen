#pragma once

#include <lua.hpp>

#include <optional>

#include "haylen/lua/Type.hpp"
#include "haylen/math/Cylindrical.hpp"
#include "haylen/math/Spherical.hpp"
#include "haylen/math/SphericalHarmonics.hpp"
#include "haylen/math/Vec3.hpp"

namespace haylen::lua {

template <> struct Type<haylen::math::Spherical> {
    static constexpr const char* name = "haylen.Spherical";
    using Storage = haylen::math::Spherical;
};

template <> struct Type<haylen::math::Cylindrical> {
    static constexpr const char* name = "haylen.Cylindrical";
    using Storage = haylen::math::Cylindrical;
};

template <> struct Type<haylen::math::SphericalHarmonics> {
    static constexpr const char* name = "haylen.SphericalHarmonics";
    using Storage = haylen::math::SphericalHarmonics;
};

} // namespace haylen::lua

namespace haylen::lua::math {

// Installs the spherical and cylindrical coordinates and the spherical harmonics of `haylen.math`: the classes `Spherical`, `Cylindrical` and `SphericalHarmonics` with their constructors.
class SphericalBinding final {
  public:
    static void install(lua_State* L);

    // Sets `spherical`, `cylindrical`, `sphericalHarmonics`, `sphericalHarmonicsFromCubemap` and `sphericalHarmonicsFromPanorama` on the module table at the top of the stack.
    static void addFunctions(lua_State* L);

  private:
    static int newSpherical(lua_State* L);
    static int newCylindrical(lua_State* L);
    static int newHarmonics(lua_State* L);
    static int harmonicsFromCubemap(lua_State* L);
    static int harmonicsFromPanorama(lua_State* L);

    static int sphericalToString(lua_State* L);
    static int cylindricalToString(lua_State* L);

    static int harmonicsProjectDirection(lua_State* L);
    static int harmonicsCoefficients(lua_State* L);
    static int harmonicsAdd(lua_State* L);
    static int harmonicsMultiply(lua_State* L);
    static int harmonicsEqual(lua_State* L);
    static int harmonicsToString(lua_State* L);
    [[nodiscard]] static haylen::math::Color harmonicsEvaluate(const haylen::math::SphericalHarmonics& self, haylen::math::Vec3 direction);
    [[nodiscard]] static haylen::math::SphericalHarmonics harmonicsConvolveLambert(const haylen::math::SphericalHarmonics& self);
    [[nodiscard]] static haylen::math::SphericalHarmonics harmonicsLerp(const haylen::math::SphericalHarmonics& self, const haylen::math::SphericalHarmonics& to, float t);
};

} // namespace haylen::lua::math
