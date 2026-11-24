#pragma once

#include <lua.hpp>

#include <optional>
#include <vector>

#include "haylen/math/Box3.hpp"
#include "haylen/math/Capsule3.hpp"
#include "haylen/math/Frustum.hpp"
#include "haylen/math/Mat4.hpp"
#include "haylen/math/Obb.hpp"
#include "haylen/math/Plane.hpp"
#include "haylen/math/Quat.hpp"
#include "haylen/math/Ray3.hpp"
#include "haylen/math/RayHit3.hpp"
#include "haylen/math/Segment3.hpp"
#include "haylen/math/Sphere.hpp"
#include "haylen/math/Triangle3.hpp"
#include "haylen/math/Vec3.hpp"

namespace haylen::lua::math {

// Installs the 3D shapes of `haylen.math`: `Plane`, `Ray3`, `Segment3`, `Box3`, `Sphere`, `Obb`, `Capsule3`, `Triangle3` and `Frustum`, with their ray casts and overlap tests.
class Geometry3Binding final {
  public:
    static void install(lua_State* L);

    // Sets the constructors of the 3D shapes on the module table at the top of the stack.
    static void addFunctions(lua_State* L);

  private:
    static int planeToString(lua_State* L);
    static int planeEqual(lua_State* L);
    static void checkNormal(haylen::math::Vec3 normal);
    static void checkDirection(haylen::math::Vec3 direction);
    static void checkRadius(float radius);
    static void checkExtents(haylen::math::Vec3 halfExtents);
    [[nodiscard]] static haylen::math::Ray3 newRay(haylen::math::Vec3 origin, haylen::math::Vec3 direction, std::optional<float> length);
    [[nodiscard]] static haylen::math::Segment3 newSegment(haylen::math::Vec3 start, haylen::math::Vec3 end);
    [[nodiscard]] static haylen::math::Box3 newBox(haylen::math::Vec3 minimum, haylen::math::Vec3 maximum);
    [[nodiscard]] static haylen::math::Box3 boxBounds(std::vector<haylen::math::Vec3> points);
    [[nodiscard]] static haylen::math::Sphere newSphere(haylen::math::Vec3 center, float radius);
    [[nodiscard]] static haylen::math::Obb newObb(haylen::math::Vec3 center, std::optional<haylen::math::Quat> rotation, haylen::math::Vec3 halfExtents);
    [[nodiscard]] static haylen::math::Capsule3 newCapsule(haylen::math::Vec3 start, haylen::math::Vec3 end, float radius);
    [[nodiscard]] static haylen::math::Triangle3 newTriangle(haylen::math::Vec3 a, haylen::math::Vec3 b, haylen::math::Vec3 c);
    [[nodiscard]] static haylen::math::Frustum newFrustum(const haylen::math::Mat4& viewProjection, std::optional<haylen::math::Mat4::DepthRange> depthRange);
    static void pushHit(lua_State* L, const std::optional<haylen::math::RayHit3>& hit);
    static int rayCast(lua_State* L);
    static int boxContains(lua_State* L);
    static int boxIntersects(lua_State* L);
    static int boxMerged(lua_State* L);
    [[nodiscard]] static haylen::math::Box3 boxTransformed(const haylen::math::Box3& self, const haylen::math::Mat4& placement);
    static int sphereIntersects(lua_State* L);
    [[nodiscard]] static bool obbIntersects(const haylen::math::Obb& self, const haylen::math::Obb& other);
    [[nodiscard]] static haylen::math::Vec3 triangleNormal(const haylen::math::Triangle3& self);
    static int segmentClosestPoints(lua_State* L);
    static int frustumPlanes(lua_State* L);
    [[nodiscard]] static bool frustumContains(const haylen::math::Frustum& self, haylen::math::Vec3 point, std::optional<float> tolerance);
    static int frustumClassify(lua_State* L);
};

} // namespace haylen::lua::math
