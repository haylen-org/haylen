#pragma once

#include <memory>
#include <string>

#include "haylen/3d/navigation/NavMesh.hpp"
#include "haylen/core/JobSystem.hpp"
#include "haylen/lua/Promise.hpp"
#include "haylen/lua/Type.hpp"

struct lua_State;

namespace haylen::core {
class Engine;
}

namespace haylen::lua {

template <> struct Type<haylen::navigation3d::NavMesh> {
    static constexpr const char* name = "haylen.NavMesh3D";
    using Storage = std::shared_ptr<haylen::navigation3d::NavMesh>;
};

} // namespace haylen::lua

namespace haylen::lua::navigation3d {

// Installs the `NavMesh3D` class: the queries of a navigation mesh, its obstacles and links, the rebuild of its tiles and its bytes.
class NavMeshBinding final {
  public:
    static void install(lua_State* L);

    // Pushes a mesh that the plugin of 3D navigation follows, so its tiles rebuild in the frames that come.
    static void push(lua_State* L, std::shared_ptr<haylen::navigation3d::NavMesh> mesh);

    // Bakes with `build(geometry[, settings])` and `buildAsync(geometry[, settings])`, and reads bytes with `decode(bytes[, name])`.
    static int build(lua_State* L);
    static int buildAsync(lua_State* L);
    static int decode(lua_State* L);

  private:
    [[nodiscard]] static haylen::navigation3d::NavMesh& check(lua_State* L);
    static void settleBuild(haylen::core::Engine& engine, const lua::Promise& promise, haylen::core::JobSystem::Result<haylen::navigation3d::NavMesh> result);
    static void settleUpdate(const lua::Promise& promise, std::string error);

    static int findNearest(lua_State* L);
    static int findPath(lua_State* L);
    static int raycast(lua_State* L);
    static int moveAlongSurface(lua_State* L);
    static int findDistanceToWall(lua_State* L);
    static int findRandomPoint(lua_State* L);
    static int findRandomPointAround(lua_State* L);
    static int newFilter(lua_State* L);
    static int area(lua_State* L);
    static int addObstacle(lua_State* L);
    static int setObstacle(lua_State* L);
    static int removeObstacle(lua_State* L);
    static int obstacle(lua_State* L);
    static int addLink(lua_State* L);
    static int removeLink(lua_State* L);
    static int link(lua_State* L);
    static int rebuildAsync(lua_State* L);
    static int updatedAsync(lua_State* L);
    static int encode(lua_State* L);
    static int triangles(lua_State* L);
    static int tileCount(lua_State* L);
    static int polygonCount(lua_State* L);
    static int obstacleCount(lua_State* L);
    static int linkCount(lua_State* L);
    static int areas(lua_State* L);
    static int bounds(lua_State* L);
    static int settings(lua_State* L);
    static int upToDate(lua_State* L);
    static int pendingTiles(lua_State* L);
};

} // namespace haylen::lua::navigation3d
