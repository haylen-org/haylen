#pragma once

#include <lua.hpp>

#include <optional>

#include "haylen/math/RayHit.hpp"

namespace haylen::math {

// Adds the ray casts of `math::Raycast` to `haylen.math`. Hits cross into Lua as `{x, y, normalX, normalY, distance, fraction}` tables, which the ray casts of other modules extend with fields of their own.
class RaycastLua final {
  public:
    // Sets the ray cast functions on the module table at the top of the stack.
    static void addFunctions(lua_State* L);

    // Pushes a hit table for a ray of the given length. The fraction is the distance over that length, or zero for rays without an end.
    static void pushHit(lua_State* L, const RayHit& hit, float length);

    // Sets the 1-based index field of the hit table on top of the stack.
    static void setIndex(lua_State* L, const RayHit& hit);

  private:
    // Pushes `nil` when there is no hit, and otherwise the hit with its index when the cast targets a list.
    static void pushResult(lua_State* L, const std::optional<RayHit>& hit, float length, bool indexed);

    static int raycastSegment(lua_State* L);
    static int raycastRect(lua_State* L);
    static int raycastCircle(lua_State* L);
    static int raycastPolygon(lua_State* L);
    static int raycastChain(lua_State* L);
    static int raycastSegments(lua_State* L);
    static int raycastSegmentsAll(lua_State* L);
    static int bounceRay(lua_State* L);
    static int rayFan(lua_State* L);
};

} // namespace haylen::math
