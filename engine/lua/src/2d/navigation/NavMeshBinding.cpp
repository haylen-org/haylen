#include "2d/navigation/NavMeshBinding.hpp"

#include <memory>
#include <optional>
#include <span>
#include <utility>

#include "haylen/2d/navigation/NavMesh.hpp"
#include "haylen/core/Engine.hpp"
#include "haylen/core/JobSystem.hpp"
#include "haylen/lua/Binding.hpp"
#include "haylen/lua/ClassBuilder.hpp"
#include "haylen/lua/Promise.hpp"
#include "haylen/lua/Runtime.hpp"
#include "haylen/lua/Stack.hpp"
#include "haylen/lua/Type.hpp"
#include "haylen/lua/TypeConverter.hpp"
#include "haylen/lua/Userdata.hpp"

namespace haylen::lua {

template <> struct Type<haylen::navigation2d::NavMesh> {
    static constexpr const char* name = "haylen.NavMesh";
    using Storage = haylen::navigation2d::NavMesh;
};

} // namespace haylen::lua

namespace haylen::lua::navigation2d {

haylen::navigation2d::NavMesh& NavMeshBinding::check(lua_State* L) {
    return lua::Userdata::check<haylen::navigation2d::NavMesh>(L, 1);
}

std::vector<std::vector<haylen::math::Vec2>> NavMeshBinding::readPolygons(lua_State* L, int index) {
    if (lua_isnoneornil(L, index)) {
        return {};
    }

    return lua::Stack::read<std::vector<std::vector<haylen::math::Vec2>>>(L, index);
}

// Creates a mesh with `newNavMesh([boundary])`, whose boundary may also come later from `setBoundary`.
int NavMeshBinding::newNavMesh(lua_State* L) {
    haylen::navigation2d::NavMesh mesh;
    if (!lua_isnoneornil(L, 1)) {
        mesh.setBoundary(lua::Stack::read<std::vector<haylen::math::Vec2>>(L, 1));
    }

    lua::Userdata::emplace<haylen::navigation2d::NavMesh>(L, std::move(mesh));
    return 1;
}

// Returns a promise for a mesh built in the background with `buildNavMeshAsync(boundary[, obstacles])`.
int NavMeshBinding::buildAsync(lua_State* L) {
    haylen::navigation2d::NavMesh mesh;
    mesh.setBoundary(lua::Stack::read<std::vector<haylen::math::Vec2>>(L, 1));
    for (const std::vector<haylen::math::Vec2>& obstacle : readPolygons(L, 2)) {
        mesh.addObstacle(obstacle);
    }

    haylen::core::Engine& engine = lua::Runtime::getEngine(L);
    const lua::Promise promise(engine);

    // clang-format off
    engine.getJobs().run([mesh = std::move(mesh)]() mutable {
        mesh.build();
        return std::move(mesh);
    }, [promise](haylen::core::JobSystem::Result<haylen::navigation2d::NavMesh> result) {
        promise.settle(std::move(result), &pushCopy);
    });
    // clang-format on
    promise.push(L);
    return 1;
}

// The promise pushes the mesh once for every coroutine that awaits it, and each one gets its own copy.
void NavMeshBinding::pushCopy(lua_State* L, const haylen::navigation2d::NavMesh& mesh) {
    lua::Userdata::emplace<haylen::navigation2d::NavMesh>(L, mesh);
}

int NavMeshBinding::setBoundary(lua_State* L) {
    check(L).setBoundary(lua::Stack::read<std::vector<haylen::math::Vec2>>(L, 2));
    return 0;
}

int NavMeshBinding::getBoundary(lua_State* L) {
    lua::Stack::push(L, check(L).getBoundary());
    return 1;
}

int NavMeshBinding::addObstacle(lua_State* L) {
    lua::Stack::push(L, check(L).addObstacle(lua::Stack::read<std::vector<haylen::math::Vec2>>(L, 2)));
    return 1;
}

int NavMeshBinding::setObstacle(lua_State* L) {
    check(L).setObstacle(lua::Stack::read<std::uint32_t>(L, 2), lua::Stack::read<std::vector<haylen::math::Vec2>>(L, 3));
    return 0;
}

int NavMeshBinding::removeObstacle(lua_State* L) {
    lua::Stack::push(L, check(L).removeObstacle(lua::Stack::read<std::uint32_t>(L, 2)));
    return 1;
}

int NavMeshBinding::clearObstacles(lua_State* L) {
    check(L).clearObstacles();
    return 0;
}

int NavMeshBinding::build(lua_State* L) {
    check(L).build();
    return 0;
}

// Finds a path with `findPath(x1, y1, x2, y2[, agentRadius])` and returns its points and length, or `nil` when there is none.
int NavMeshBinding::findPath(lua_State* L) {
    haylen::navigation2d::NavMesh& mesh = check(L);
    const float radius = lua_isnoneornil(L, 6) ? 0.0F : lua::Stack::read<float>(L, 6);
    const std::span<const haylen::math::Vec2> path = mesh.findPath({lua::Stack::read<float>(L, 2), lua::Stack::read<float>(L, 3)}, {lua::Stack::read<float>(L, 4), lua::Stack::read<float>(L, 5)}, radius);
    if (path.empty()) {
        lua_pushnil(L);
        return 1;
    }

    lua::Stack::push(L, std::vector<haylen::math::Vec2>(path.begin(), path.end()));
    lua::Stack::push(L, mesh.getPathLength());
    return 2;
}

// Returns the 1-based index of the triangle under `x`, `y` with `findTriangle(x, y)`, or `nil` outside the mesh.
int NavMeshBinding::findTriangle(lua_State* L) {
    const std::optional<std::size_t> triangle = check(L).findTriangle({lua::Stack::read<float>(L, 2), lua::Stack::read<float>(L, 3)});
    if (!triangle) {
        lua_pushnil(L);
        return 1;
    }

    lua::Stack::push(L, *triangle + 1);
    return 1;
}

int NavMeshBinding::contains(lua_State* L) {
    lua::Stack::push(L, check(L).contains({lua::Stack::read<float>(L, 2), lua::Stack::read<float>(L, 3)}));
    return 1;
}

int NavMeshBinding::closestPoint(lua_State* L) {
    const std::optional<haylen::math::Vec2> point = check(L).getClosestPoint({lua::Stack::read<float>(L, 2), lua::Stack::read<float>(L, 3)});
    if (!point) {
        lua_pushnil(L);
        return 1;
    }

    lua::Stack::push(L, point->x);
    lua::Stack::push(L, point->y);
    return 2;
}

// Lists the triangles as lists of three `Vec2` corners, for drawing the mesh.
int NavMeshBinding::triangles(lua_State* L) {
    haylen::navigation2d::NavMesh& mesh = check(L);
    const std::vector<haylen::navigation2d::NavMesh::Triangle>& all = mesh.getTriangles();
    const std::vector<haylen::math::Vec2>& corners = mesh.getVertices();
    lua_createtable(L, static_cast<int>(all.size()), 0);
    for (std::size_t index = 0; index < all.size(); ++index) {
        lua_createtable(L, 3, 0);
        for (std::size_t corner = 0; corner < 3; ++corner) {
            lua::Stack::push(L, corners[all[index].vertices[corner]]);
            lua_rawseti(L, -2, static_cast<lua_Integer>(corner + 1));
        }

        lua_rawseti(L, -2, static_cast<lua_Integer>(index + 1));
    }

    return 1;
}

int NavMeshBinding::obstacleCount(lua_State* L) {
    lua::Stack::push(L, check(L).getObstacleCount());
    return 1;
}

int NavMeshBinding::triangleCount(lua_State* L) {
    lua::Stack::push(L, check(L).getTriangles().size());
    return 1;
}

int NavMeshBinding::isDirty(lua_State* L) {
    lua::Stack::push(L, check(L).isDirty());
    return 1;
}

void NavMeshBinding::addFunctions(lua_State* L) {
    const luaL_Reg functions[] = {
        {"newNavMesh", &lua::Binding::native<&newNavMesh>},
        {"buildNavMeshAsync", &lua::Binding::native<&buildAsync>},
        {nullptr, nullptr},
    };
    luaL_setfuncs(L, functions, 0);
}

void NavMeshBinding::install(lua_State* L) {
    lua::ClassBuilder<haylen::navigation2d::NavMesh>(L).function("setBoundary", &lua::Binding::native<&setBoundary>).function("addObstacle", &lua::Binding::native<&addObstacle>).function("setObstacle", &lua::Binding::native<&setObstacle>).function("removeObstacle", &lua::Binding::native<&removeObstacle>).function("clearObstacles", &lua::Binding::native<&clearObstacles>).function("build", &lua::Binding::native<&build>).function("findPath", &lua::Binding::native<&findPath>).function("findTriangle", &lua::Binding::native<&findTriangle>).function("contains", &lua::Binding::native<&contains>).function("closestPoint", &lua::Binding::native<&closestPoint>).function("triangles", &lua::Binding::native<&triangles>).property("boundary", &getBoundary).property("obstacleCount", &obstacleCount).property("triangleCount", &lua::Binding::native<&triangleCount>).property("dirty", &isDirty).install();
}

} // namespace haylen::lua::navigation2d
