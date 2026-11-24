#pragma once

#include <lua.hpp>

#include <optional>

#include "haylen/math/Mat3.hpp"
#include "haylen/math/Quat.hpp"
#include "haylen/math/Vec3.hpp"

namespace haylen::lua::math {

// Installs the `Quat` class of `haylen.math` with its constructors from an axis and an angle, Euler angles, two directions and a direction to look along.
class QuatBinding final {
  public:
    static void install(lua_State* L);

    // Sets `quat`, `fromAxisAngle`, `fromEuler`, `rotationBetween` and `lookRotation` on the module table at the top of the stack.
    static void addFunctions(lua_State* L);

  private:
    static int newQuat(lua_State* L);
    [[nodiscard]] static haylen::math::Quat fromEuler(haylen::math::Vec3 angles, std::optional<haylen::math::Quat::EulerOrder> order);
    [[nodiscard]] static haylen::math::Quat lookRotation(haylen::math::Vec3 direction, std::optional<haylen::math::Vec3> up);
    static int multiply(lua_State* L);
    static int equal(lua_State* L);
    static int toString(lua_State* L);
    static int unpack(lua_State* L);
    static int swingTwist(lua_State* L);
    [[nodiscard]] static haylen::math::Vec3 rotate(const haylen::math::Quat& self, haylen::math::Vec3 vector);
    [[nodiscard]] static haylen::math::Quat normalized(const haylen::math::Quat& self);
    [[nodiscard]] static haylen::math::Quat inverse(const haylen::math::Quat& self);
    [[nodiscard]] static haylen::math::Vec3 axis(const haylen::math::Quat& self);
    [[nodiscard]] static haylen::math::Vec3 toEuler(const haylen::math::Quat& self, std::optional<haylen::math::Quat::EulerOrder> order);
    [[nodiscard]] static bool approximately(const haylen::math::Quat& self, const haylen::math::Quat& other, std::optional<float> epsilon);
};

} // namespace haylen::lua::math
