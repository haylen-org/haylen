#pragma once

#include <lua.hpp>

#include <optional>

#include "haylen/math/DualQuat.hpp"
#include "haylen/math/Quat.hpp"
#include "haylen/math/Transform3D.hpp"
#include "haylen/math/Vec3.hpp"

namespace haylen::lua::math {

// Installs the `Transform3D` and `DualQuat` classes of `haylen.math` with their constructors and the blend of dual quaternions.
class Transform3DBinding final {
  public:
    static void install(lua_State* L);

    // Sets `transform3d`, `dualQuat` and `blendDualQuats` on the module table at the top of the stack.
    static void addFunctions(lua_State* L);

  private:
    static int newTransform(lua_State* L);
    [[nodiscard]] static haylen::math::DualQuat newDualQuat(std::optional<haylen::math::Quat> rotation, std::optional<haylen::math::Vec3> translation);
    static int blendDualQuats(lua_State* L);
    static int transformMultiply(lua_State* L);
    static int transformEqual(lua_State* L);
    static int transformToString(lua_State* L);
    [[nodiscard]] static haylen::math::Vec3 applyNormal(const haylen::math::Transform3D& self, haylen::math::Vec3 normal);
    [[nodiscard]] static haylen::math::Vec3 applyInverse(const haylen::math::Transform3D& self, haylen::math::Vec3 point);
    [[nodiscard]] static haylen::math::Vec3 applyInverseVector(const haylen::math::Transform3D& self, haylen::math::Vec3 vector);
    [[nodiscard]] static haylen::math::Transform3D inverse(const haylen::math::Transform3D& self);
    [[nodiscard]] static bool transformApproximately(const haylen::math::Transform3D& self, const haylen::math::Transform3D& other, std::optional<float> epsilon);
    static int dualQuatMultiply(lua_State* L);
    static int dualQuatToString(lua_State* L);
    [[nodiscard]] static haylen::math::DualQuat dualQuatNormalized(const haylen::math::DualQuat& self);
    [[nodiscard]] static bool dualQuatApproximately(const haylen::math::DualQuat& self, const haylen::math::DualQuat& other, std::optional<float> epsilon);
};

} // namespace haylen::lua::math
