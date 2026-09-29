#pragma once

#include <lua.hpp>

#include <cstdint>
#include <vector>

#include "haylen/math/Vec2.hpp"

namespace haylen::navigation2d {

class NavMesh;

// Installs the NavMesh class of haylen.navigation2d. Polygons cross from Lua as lists of points, and obstacle lists as lists of polygons.
class NavMeshLua final {
  public:
    static void install(lua_State* L);

    // Sets newNavMesh and buildNavMeshAsync on the module table at the top of the stack.
    static void addFunctions(lua_State* L);

  private:
    [[nodiscard]] static NavMesh& check(lua_State* L);
    [[nodiscard]] static std::vector<std::vector<math::Vec2>> readPolygons(lua_State* L, int index);

    static int newNavMesh(lua_State* L);
    static int buildAsync(lua_State* L);
    static int setBoundary(lua_State* L);
    static int getBoundary(lua_State* L);
    static int addObstacle(lua_State* L);
    static int setObstacle(lua_State* L);
    static int removeObstacle(lua_State* L);
    static int clearObstacles(lua_State* L);
    static int build(lua_State* L);
    static int findPath(lua_State* L);
    static int findTriangle(lua_State* L);
    static int contains(lua_State* L);
    static int closestPoint(lua_State* L);
    static int triangles(lua_State* L);
    static int obstacleCount(lua_State* L);
    static int triangleCount(lua_State* L);
    static int isDirty(lua_State* L);
};

} // namespace haylen::navigation2d
