#pragma once

#include <lua.hpp>

#include <array>
#include <optional>
#include <string_view>

#include "haylen/2d/physics/RaycastHit.hpp"
#include "haylen/2d/physics/Raycaster.hpp"
#include "haylen/math/Vec2.hpp"

namespace haylen::physics2d {

// Binds the casts of physics2d::Raycaster as methods of PhysicsWorld, the RayBatch class and the ray debug drawing. Hits cross into Lua as {shape, body, x, y, normalX, normalY, fraction, distance} tables.
class WorldRaycastLua final {
  public:
    static void install(lua_State* L);

    // Sets newRayBatch and drawRay on the module table at the top of the stack.
    static void addFunctions(lua_State* L);

    static int raycast(lua_State* L);
    static int raycastAll(lua_State* L);
    static int castCircle(lua_State* L);
    static int castBox(lua_State* L);
    static int castCapsule(lua_State* L);
    static int castPolygon(lua_State* L);
    static int bounceRay(lua_State* L);
    static int rayFan(lua_State* L);
    static int lineOfSight(lua_State* L);
    static int raycastBatch(lua_State* L);
    static int pick(lua_State* L);
    static int isDebuggingRays(lua_State* L);
    static int setDebuggingRays(lua_State* L);
    static int debugDrawRays(lua_State* L);

  private:
    static constexpr std::array<std::string_view, 4> kFilterFields{"category", "mask", "group", "accept"};
    static constexpr std::array<std::string_view, 1> kLimitFields{"limit"};
    static constexpr std::array<std::string_view, 2> kPickFields{"category", "mask"};

    // Reads the filter at index, whose accept function, when given, receives each candidate hit table of the world at index 1.
    [[nodiscard]] static Raycaster::Filter readFilter(lua_State* L, int index, bool withLimit = false);
    [[nodiscard]] static std::size_t readLimit(lua_State* L, int index);
    static void pushHit(lua_State* L, int worldIndex, const RaycastHit& hit);
    static void pushResult(lua_State* L, const std::optional<RaycastHit>& hit);

    // Records a cast of the world at index 1 for debugDrawRays when debugRays is on.
    static void record(lua_State* L, math::Vec2 from, math::Vec2 to, const std::optional<RaycastHit>& hit);

    static int newRayBatch(lua_State* L);
    static int batchSize(lua_State* L);
    static int setBatchSize(lua_State* L);
    static int batchSetRay(lua_State* L);
    static int batchRay(lua_State* L);
    static int batchHit(lua_State* L);
    static int batchShape(lua_State* L);
    static int batchBody(lua_State* L);
    [[nodiscard]] static std::size_t readBatchIndex(lua_State* L, int index);
    static int drawRay(lua_State* L);
};

} // namespace haylen::physics2d
