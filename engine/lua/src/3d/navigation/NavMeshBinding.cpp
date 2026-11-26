#include "3d/navigation/NavMeshBinding.hpp"

#include <lua.hpp>

#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include "3d/navigation/GeometryBinding.hpp"
#include "3d/navigation/NavigationOptions.hpp"
#include "3d/navigation/QueryFilterBinding.hpp"
#include "core/FloatBufferBinding.hpp"
#include "haylen/core/Engine.hpp"
#include "haylen/core/FloatBuffer.hpp"
#include "haylen/lua/Binding.hpp"
#include "haylen/lua/ClassBuilder.hpp"
#include "haylen/lua/Promise.hpp"
#include "haylen/lua/Runtime.hpp"
#include "haylen/lua/Stack.hpp"
#include "haylen/lua/TypeConverter.hpp"
#include "haylen/lua/Userdata.hpp"
#include "haylen/plugins/Navigation3DPlugin.hpp"

namespace haylen::lua::navigation3d {

haylen::navigation3d::NavMesh& NavMeshBinding::check(lua_State* L) {
    return lua::Userdata::check<haylen::navigation3d::NavMesh>(L, 1);
}

void NavMeshBinding::push(lua_State* L, std::shared_ptr<haylen::navigation3d::NavMesh> mesh) {
    lua::Runtime::getEngine(L).getPlugin<haylen::plugins::Navigation3DPlugin>().track(mesh);
    lua::Userdata::emplace<haylen::navigation3d::NavMesh>(L, std::move(mesh));
}

int NavMeshBinding::build(lua_State* L) {
    const haylen::navigation3d::SourceGeometry& geometry = lua::Userdata::check<haylen::navigation3d::SourceGeometry>(L, 1);
    const haylen::navigation3d::BuildSettings settings = NavigationOptions::readSettings(L, 2);
    push(L, std::make_shared<haylen::navigation3d::NavMesh>(haylen::navigation3d::NavMesh::build(geometry, settings)));
    return 1;
}

// The bake reads a copy of the geometry, so the app may change its geometry while the bake runs.
int NavMeshBinding::buildAsync(lua_State* L) {
    const haylen::navigation3d::SourceGeometry& geometry = lua::Userdata::check<haylen::navigation3d::SourceGeometry>(L, 1);
    const haylen::navigation3d::BuildSettings settings = NavigationOptions::readSettings(L, 2);
    haylen::core::Engine& engine = lua::Runtime::getEngine(L);
    const lua::Promise promise(engine);

    // clang-format off
    haylen::navigation3d::NavMesh::buildAsync(engine.getJobs(), std::make_shared<const haylen::navigation3d::SourceGeometry>(geometry), settings, [&engine, promise](haylen::core::JobSystem::Result<haylen::navigation3d::NavMesh> result) {
        settleBuild(engine, promise, std::move(result));
    });
    // clang-format on
    promise.push(L);
    return 1;
}

// Every coroutine that awaits the bake receives the same mesh.
void NavMeshBinding::settleBuild(haylen::core::Engine& engine, const lua::Promise& promise, haylen::core::JobSystem::Result<haylen::navigation3d::NavMesh> result) {
    if (!result.isOk()) {
        promise.reject(std::move(result.error));
        return;
    }

    auto mesh = std::make_shared<haylen::navigation3d::NavMesh>(std::move(*result.value));
    engine.getPlugin<haylen::plugins::Navigation3DPlugin>().track(mesh);
    promise.resolveWith([mesh](lua_State* state) { lua::Userdata::emplace<haylen::navigation3d::NavMesh>(state, mesh); });
}

int NavMeshBinding::decode(lua_State* L) {
    std::size_t size = 0;
    const char* bytes = luaL_checklstring(L, 1, &size);
    const std::string name = lua_isnoneornil(L, 2) ? std::string("bytes") : lua::Stack::read<std::string>(L, 2);
    const std::span<const std::uint8_t> view(reinterpret_cast<const std::uint8_t*>(bytes), size);
    push(L, std::make_shared<haylen::navigation3d::NavMesh>(haylen::navigation3d::NavMesh::decode(view, name)));
    return 1;
}

// Snaps a point to the mesh with `findNearest(point[, options])` and returns `{position, polygon, area}`, or `nil`.
int NavMeshBinding::findNearest(lua_State* L) {
    const haylen::navigation3d::NavMesh& self = check(L);
    const haylen::navigation3d::NavMesh::QueryOptions options = NavigationOptions::readQuery(L, 3);
    NavigationOptions::pushLocation(L, self, self.findNearest(lua::Stack::read<haylen::math::Vec3>(L, 2), options));
    return 1;
}

// Finds a path with `findPath(start, goal[, options])` and returns `{points, links, partial, length}`, or `nil` when the start or the goal is off the mesh.
int NavMeshBinding::findPath(lua_State* L) {
    const haylen::navigation3d::NavMesh& self = check(L);
    const haylen::navigation3d::NavMesh::QueryOptions options = NavigationOptions::readQuery(L, 4);
    const std::optional<haylen::navigation3d::NavMesh::Path> path = self.findPath(lua::Stack::read<haylen::math::Vec3>(L, 2), lua::Stack::read<haylen::math::Vec3>(L, 3), options);
    if (!path) {
        lua_pushnil(L);
        return 1;
    }

    lua_createtable(L, 0, 4);
    lua::Stack::push(L, path->points);
    lua_setfield(L, -2, "points");
    lua_createtable(L, static_cast<int>(path->links.size()), 0);
    for (std::size_t index = 0; index < path->links.size(); ++index) {
        lua_pushinteger(L, static_cast<lua_Integer>(path->links[index]));
        lua_rawseti(L, -2, static_cast<lua_Integer>(index + 1));
    }
    lua_setfield(L, -2, "links");
    lua::Stack::push(L, path->partial);
    lua_setfield(L, -2, "partial");
    lua::Stack::push(L, path->length);
    lua_setfield(L, -2, "length");
    return 1;
}

// Casts a ray along the surface with `raycast(start, end[, options])` and returns `{hit, fraction, position, normal}`, or `nil` when the start is off the mesh.
int NavMeshBinding::raycast(lua_State* L) {
    const haylen::navigation3d::NavMesh& self = check(L);
    const haylen::navigation3d::NavMesh::QueryOptions options = NavigationOptions::readQuery(L, 4);
    const std::optional<haylen::navigation3d::NavMesh::RayHit> hit = self.raycast(lua::Stack::read<haylen::math::Vec3>(L, 2), lua::Stack::read<haylen::math::Vec3>(L, 3), options);
    if (!hit) {
        lua_pushnil(L);
        return 1;
    }

    lua_createtable(L, 0, 4);
    lua::Stack::push(L, hit->hit);
    lua_setfield(L, -2, "hit");
    lua::Stack::push(L, hit->fraction);
    lua_setfield(L, -2, "fraction");
    lua::Stack::push(L, hit->position);
    lua_setfield(L, -2, "position");
    if (hit->hit) {
        lua::Stack::push(L, hit->normal);
        lua_setfield(L, -2, "normal");
    }
    return 1;
}

int NavMeshBinding::moveAlongSurface(lua_State* L) {
    const haylen::navigation3d::NavMesh& self = check(L);
    const haylen::navigation3d::NavMesh::QueryOptions options = NavigationOptions::readQuery(L, 4);
    NavigationOptions::pushLocation(L, self, self.moveAlongSurface(lua::Stack::read<haylen::math::Vec3>(L, 2), lua::Stack::read<haylen::math::Vec3>(L, 3), options));
    return 1;
}

// Finds the closest wall with `findDistanceToWall(point, radius[, options])` and returns `{distance, position, normal}`, or `nil` when no wall is within the radius.
int NavMeshBinding::findDistanceToWall(lua_State* L) {
    const haylen::navigation3d::NavMesh& self = check(L);
    const haylen::navigation3d::NavMesh::QueryOptions options = NavigationOptions::readQuery(L, 4);
    const std::optional<haylen::navigation3d::NavMesh::WallHit> wall = self.findDistanceToWall(lua::Stack::read<haylen::math::Vec3>(L, 2), lua::Stack::read<float>(L, 3), options);
    if (!wall) {
        lua_pushnil(L);
        return 1;
    }

    lua_createtable(L, 0, 3);
    lua::Stack::push(L, wall->distance);
    lua_setfield(L, -2, "distance");
    lua::Stack::push(L, wall->position);
    lua_setfield(L, -2, "position");
    lua::Stack::push(L, wall->normal);
    lua_setfield(L, -2, "normal");
    return 1;
}

int NavMeshBinding::findRandomPoint(lua_State* L) {
    const haylen::navigation3d::NavMesh& self = check(L);
    const haylen::navigation3d::NavMesh::QueryOptions options = NavigationOptions::readQuery(L, 2, {NavigationOptions::kRandomFields});
    haylen::math::Random own;
    haylen::math::Random& random = NavigationOptions::readRandom(L, 2, own);
    NavigationOptions::pushLocation(L, self, self.findRandomPoint(random, options));
    return 1;
}

int NavMeshBinding::findRandomPointAround(lua_State* L) {
    const haylen::navigation3d::NavMesh& self = check(L);
    const haylen::navigation3d::NavMesh::QueryOptions options = NavigationOptions::readQuery(L, 4, {NavigationOptions::kRandomFields});
    haylen::math::Random own;
    haylen::math::Random& random = NavigationOptions::readRandom(L, 4, own);
    NavigationOptions::pushLocation(L, self, self.findRandomPointAround(lua::Stack::read<haylen::math::Vec3>(L, 2), lua::Stack::read<float>(L, 3), random, options));
    return 1;
}

int NavMeshBinding::newFilter(lua_State* L) {
    QueryFilterBinding::create(L, check(L), 2);
    return 1;
}

// Returns the number of an area with `area(name)`.
int NavMeshBinding::area(lua_State* L) {
    lua_pushinteger(L, check(L).getArea(lua::Stack::read<std::string_view>(L, 2)));
    return 1;
}

int NavMeshBinding::addObstacle(lua_State* L) {
    lua_pushinteger(L, check(L).addObstacle(NavigationOptions::readObstacle(L, 2)));
    return 1;
}

int NavMeshBinding::setObstacle(lua_State* L) {
    check(L).setObstacle(lua::Stack::read<std::uint32_t>(L, 2), NavigationOptions::readObstacle(L, 3));
    return 0;
}

int NavMeshBinding::removeObstacle(lua_State* L) {
    lua::Stack::push(L, check(L).removeObstacle(lua::Stack::read<std::uint32_t>(L, 2)));
    return 1;
}

int NavMeshBinding::obstacle(lua_State* L) {
    const std::optional<haylen::navigation3d::Obstacle> found = check(L).getObstacle(lua::Stack::read<std::uint32_t>(L, 2));
    if (!found) {
        lua_pushnil(L);
        return 1;
    }

    NavigationOptions::pushObstacle(L, *found);
    return 1;
}

int NavMeshBinding::addLink(lua_State* L) {
    lua_pushinteger(L, check(L).addLink(NavigationOptions::readLink(L, 2, 3, 4)));
    return 1;
}

int NavMeshBinding::removeLink(lua_State* L) {
    lua::Stack::push(L, check(L).removeLink(lua::Stack::read<std::uint32_t>(L, 2)));
    return 1;
}

int NavMeshBinding::link(lua_State* L) {
    const std::optional<haylen::navigation3d::Link> found = check(L).getLink(lua::Stack::read<std::uint32_t>(L, 2));
    if (!found) {
        lua_pushnil(L);
        return 1;
    }

    NavigationOptions::pushLink(L, *found);
    return 1;
}

// Bakes the tiles that touch a box again with `rebuildAsync(geometry, box)`, from a copy of the geometry, and returns a promise that resolves once every tile is in place.
int NavMeshBinding::rebuildAsync(lua_State* L) {
    haylen::navigation3d::NavMesh& self = check(L);
    const haylen::navigation3d::SourceGeometry& geometry = lua::Userdata::check<haylen::navigation3d::SourceGeometry>(L, 2);
    self.rebuild(std::make_shared<const haylen::navigation3d::SourceGeometry>(geometry), lua::Stack::read<haylen::math::Box>(L, 3));
    return updatedAsync(L);
}

// Returns a promise that resolves once no tile waits for its rebuild, or rejects with the error of a rebuild that failed.
int NavMeshBinding::updatedAsync(lua_State* L) {
    haylen::navigation3d::NavMesh& self = check(L);
    const lua::Promise promise(lua::Runtime::getEngine(L));

    self.whenUpToDate([promise](std::string error) { settleUpdate(promise, std::move(error)); });
    promise.push(L);
    return 1;
}

void NavMeshBinding::settleUpdate(const lua::Promise& promise, std::string error) {
    if (!error.empty()) {
        promise.reject(std::move(error));
        return;
    }

    promise.resolveWith([](lua_State* state) { lua_pushboolean(state, 1); });
}

int NavMeshBinding::encode(lua_State* L) {
    const std::vector<std::uint8_t> bytes = check(L).encode();
    lua_pushlstring(L, reinterpret_cast<const char*>(bytes.data()), bytes.size());
    return 1;
}

// Writes the corners of the triangles of the surface into a new float buffer, nine numbers for each triangle, such as to draw the mesh.
int NavMeshBinding::triangles(lua_State* L) {
    const haylen::navigation3d::NavMesh::Triangles all = check(L).getTriangles();
    auto buffer = std::make_shared<haylen::core::FloatBuffer>(all.corners.size() * 3);
    const std::span<float> values = buffer->getValues();
    for (std::size_t corner = 0; corner < all.corners.size(); ++corner) {
        values[corner * 3] = all.corners[corner].x;
        values[corner * 3 + 1] = all.corners[corner].y;
        values[corner * 3 + 2] = all.corners[corner].z;
    }
    lua::Userdata::emplace<haylen::core::FloatBuffer>(L, std::move(buffer));
    return 1;
}

int NavMeshBinding::tileCount(lua_State* L) {
    lua::Stack::push(L, check(L).getTileCount());
    return 1;
}

int NavMeshBinding::polygonCount(lua_State* L) {
    lua::Stack::push(L, check(L).getPolygonCount());
    return 1;
}

int NavMeshBinding::obstacleCount(lua_State* L) {
    lua::Stack::push(L, check(L).getObstacleCount());
    return 1;
}

int NavMeshBinding::linkCount(lua_State* L) {
    lua::Stack::push(L, check(L).getLinkCount());
    return 1;
}

int NavMeshBinding::areas(lua_State* L) {
    lua::Stack::push(L, check(L).getAreas());
    return 1;
}

int NavMeshBinding::bounds(lua_State* L) {
    lua::Stack::push(L, check(L).getBounds());
    return 1;
}

int NavMeshBinding::settings(lua_State* L) {
    NavigationOptions::pushSettings(L, check(L).getSettings());
    return 1;
}

int NavMeshBinding::upToDate(lua_State* L) {
    lua::Stack::push(L, check(L).isUpToDate());
    return 1;
}

int NavMeshBinding::pendingTiles(lua_State* L) {
    lua::Stack::push(L, check(L).getPendingTileCount());
    return 1;
}

void NavMeshBinding::install(lua_State* L) {
    lua::ClassBuilder<haylen::navigation3d::NavMesh>(L).function("findNearest", &lua::Binding::native<&findNearest>).function("findPath", &lua::Binding::native<&findPath>).function("raycast", &lua::Binding::native<&raycast>).function("moveAlongSurface", &lua::Binding::native<&moveAlongSurface>).function("findDistanceToWall", &lua::Binding::native<&findDistanceToWall>).function("findRandomPoint", &lua::Binding::native<&findRandomPoint>).function("findRandomPointAround", &lua::Binding::native<&findRandomPointAround>).function("newFilter", &lua::Binding::native<&newFilter>).function("area", &lua::Binding::native<&area>).function("addObstacle", &lua::Binding::native<&addObstacle>).function("setObstacle", &lua::Binding::native<&setObstacle>).function("removeObstacle", &lua::Binding::native<&removeObstacle>).function("obstacle", &lua::Binding::native<&obstacle>).function("addLink", &lua::Binding::native<&addLink>).function("removeLink", &lua::Binding::native<&removeLink>).function("link", &lua::Binding::native<&link>).function("rebuildAsync", &lua::Binding::native<&rebuildAsync>).function("updatedAsync", &lua::Binding::native<&updatedAsync>).function("encode", &lua::Binding::native<&encode>).function("triangles", &lua::Binding::native<&triangles>).property("tileCount", &lua::Binding::native<&tileCount>).property("polygonCount", &lua::Binding::native<&polygonCount>).property("obstacleCount", &lua::Binding::native<&obstacleCount>).property("linkCount", &lua::Binding::native<&linkCount>).property("areas", &lua::Binding::native<&areas>).property("bounds", &lua::Binding::native<&bounds>).property("settings", &lua::Binding::native<&settings>).property("upToDate", &lua::Binding::native<&upToDate>).property("pendingTiles", &lua::Binding::native<&pendingTiles>).install();
}

} // namespace haylen::lua::navigation3d
