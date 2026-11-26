#include "3d/spatial/MeshBvhBinding.hpp"

#include <format>
#include <limits>
#include <optional>
#include <span>
#include <stdexcept>
#include <utility>

#include "3d/spatial/ScriptedMesh.hpp"
#include "core/FloatBufferBinding.hpp"
#include "haylen/core/FloatBuffer.hpp"
#include "haylen/lua/Binding.hpp"
#include "haylen/lua/ClassBuilder.hpp"
#include "haylen/lua/Stack.hpp"
#include "haylen/lua/Type.hpp"
#include "haylen/lua/TypeConverter.hpp"
#include "haylen/lua/Userdata.hpp"

namespace haylen::lua {

template <> struct Type<lua::spatial3d::ScriptedMesh> {
    static constexpr const char* name = "haylen.MeshBvh";
    using Storage = lua::spatial3d::ScriptedMesh;
};

} // namespace haylen::lua

namespace haylen::lua::spatial3d {

std::vector<float> MeshBvhBinding::readPositions(lua_State* L, int index) {
    if (const haylen::core::FloatBuffer* buffer = lua::Userdata::test<haylen::core::FloatBuffer>(L, index)) {
        const std::span<const float> values = buffer->getValues();
        return {values.begin(), values.end()};
    }

    luaL_checktype(L, index, LUA_TTABLE);
    const auto count = static_cast<std::size_t>(lua_rawlen(L, index));
    std::vector<float> positions;
    positions.reserve(count * 3);
    for (std::size_t vertex = 1; vertex <= count; ++vertex) {
        lua_rawgeti(L, index, static_cast<lua_Integer>(vertex));
        const haylen::math::Vec3 position = lua::Stack::read<haylen::math::Vec3>(L, -1);
        lua_pop(L, 1);
        positions.insert(positions.end(), {position.x, position.y, position.z});
    }

    return positions;
}

std::vector<std::uint32_t> MeshBvhBinding::readIndices(lua_State* L, int index, std::size_t vertexCount) {
    if (lua_isnoneornil(L, index)) {
        return {};
    }

    luaL_checktype(L, index, LUA_TTABLE);
    const auto count = static_cast<std::size_t>(lua_rawlen(L, index));
    std::vector<std::uint32_t> indices;
    indices.reserve(count);
    for (std::size_t item = 1; item <= count; ++item) {
        lua_rawgeti(L, index, static_cast<lua_Integer>(item));
        int whole = 0;
        const lua_Integer vertex = lua_tointegerx(L, -1, &whole);
        lua_pop(L, 1);
        if (whole == 0 || vertex < 1) {
            throw std::invalid_argument(std::format("Item {} of the indices of a mesh must name a vertex as a whole number from 1 on.", item));
        }

        if (static_cast<std::uint64_t>(vertex) > vertexCount) {
            throw std::invalid_argument(std::format("Item {} of the indices of a mesh names vertex {}, but the vertex count of the mesh is {}.", item, vertex, vertexCount));
        }

        indices.push_back(static_cast<std::uint32_t>(vertex - 1));
    }

    return indices;
}

int MeshBvhBinding::newMeshBvh(lua_State* L) {
    std::vector<float> positions = readPositions(L, 1);
    std::vector<std::uint32_t> indices = readIndices(L, 2, positions.size() / 3);
    haylen::spatial3d::MeshBvh mesh(std::move(positions), std::move(indices));
    lua::Userdata::emplace<ScriptedMesh>(L, ScriptedMesh{.mesh = std::move(mesh)});
    return 1;
}

std::uint32_t MeshBvhBinding::checkTriangle(lua_State* L, const haylen::spatial3d::MeshBvh& mesh, int index) {
    const auto triangle = lua::Stack::read<lua_Integer>(L, index);
    if (triangle < 1 || static_cast<std::uint64_t>(triangle) > mesh.getTriangleCount()) {
        throw std::invalid_argument(std::format("The triangle count of the mesh is {}, so it has no triangle {}.", mesh.getTriangleCount(), triangle));
    }

    return static_cast<std::uint32_t>(triangle - 1);
}

void MeshBvhBinding::pushTriangles(lua_State* L, const std::vector<std::uint32_t>& triangles, int results) {
    if (results > 0) {
        luaL_checktype(L, results, LUA_TTABLE);
        lua_pushvalue(L, results);
    } else {
        lua_createtable(L, static_cast<int>(triangles.size()), 0);
    }

    for (std::size_t index = 0; index < triangles.size(); ++index) {
        lua_pushinteger(L, static_cast<lua_Integer>(triangles[index]) + 1);
        lua_rawseti(L, -2, static_cast<lua_Integer>(index + 1));
    }

    // A reused table keeps no items of an earlier, longer answer.
    for (auto index = static_cast<lua_Integer>(triangles.size() + 1); results > 0; ++index) {
        const int type = lua_rawgeti(L, -1, index);
        lua_pop(L, 1);
        if (type == LUA_TNIL) {
            break;
        }

        lua_pushnil(L);
        lua_rawseti(L, -2, index);
    }
}

// Casts with `raycast(ray)` and returns the nearest hit as `{triangle, point, normal, barycentric, distance, front}`, or `nil`.
int MeshBvhBinding::raycast(lua_State* L) {
    const ScriptedMesh& self = lua::Userdata::check<ScriptedMesh>(L, 1);
    const std::optional<haylen::spatial3d::MeshBvh::Hit> hit = self.mesh.raycast(lua::Stack::read<haylen::math::Ray3D>(L, 2));
    if (!hit) {
        lua_pushnil(L);
        return 1;
    }

    lua_createtable(L, 0, 6);
    lua::Stack::push(L, static_cast<lua_Integer>(hit->triangle) + 1);
    lua_setfield(L, -2, "triangle");
    lua::Stack::push(L, hit->point);
    lua_setfield(L, -2, "point");
    lua::Stack::push(L, hit->normal);
    lua_setfield(L, -2, "normal");
    lua::Stack::push(L, hit->barycentric);
    lua_setfield(L, -2, "barycentric");
    lua::Stack::push(L, hit->distance);
    lua_setfield(L, -2, "distance");
    lua::Stack::push(L, hit->front);
    lua_setfield(L, -2, "front");
    return 1;
}

int MeshBvhBinding::intersects(lua_State* L) {
    const ScriptedMesh& self = lua::Userdata::check<ScriptedMesh>(L, 1);
    lua::Stack::push(L, self.mesh.intersects(lua::Stack::read<haylen::math::Ray3D>(L, 2)));
    return 1;
}

// Finds the closest point of the surface with `closestPoint(point[, maxDistance])` as `{triangle, point, distance}`, or `nil`.
int MeshBvhBinding::closestPoint(lua_State* L) {
    const ScriptedMesh& self = lua::Userdata::check<ScriptedMesh>(L, 1);
    const haylen::math::Vec3 point = lua::Stack::read<haylen::math::Vec3>(L, 2);
    const float maxDistance = lua_isnoneornil(L, 3) ? std::numeric_limits<float>::infinity() : lua::Stack::read<float>(L, 3);
    const std::optional<haylen::spatial3d::MeshBvh::ClosestPoint> closest = self.mesh.closestPoint(point, maxDistance);
    if (!closest) {
        lua_pushnil(L);
        return 1;
    }

    lua_createtable(L, 0, 3);
    lua::Stack::push(L, static_cast<lua_Integer>(closest->triangle) + 1);
    lua_setfield(L, -2, "triangle");
    lua::Stack::push(L, closest->point);
    lua_setfield(L, -2, "point");
    lua::Stack::push(L, closest->distance);
    lua_setfield(L, -2, "distance");
    return 1;
}

int MeshBvhBinding::queryBox(lua_State* L) {
    ScriptedMesh& self = lua::Userdata::check<ScriptedMesh>(L, 1);
    self.mesh.queryBox(lua::Stack::read<haylen::math::Box>(L, 2), self.triangles);
    pushTriangles(L, self.triangles, lua_isnoneornil(L, 3) ? 0 : 3);
    return 1;
}

int MeshBvhBinding::querySphere(lua_State* L) {
    ScriptedMesh& self = lua::Userdata::check<ScriptedMesh>(L, 1);
    self.mesh.querySphere(lua::Stack::read<haylen::math::Sphere>(L, 2), self.triangles);
    pushTriangles(L, self.triangles, lua_isnoneornil(L, 3) ? 0 : 3);
    return 1;
}

int MeshBvhBinding::triangle(lua_State* L) {
    const ScriptedMesh& self = lua::Userdata::check<ScriptedMesh>(L, 1);
    lua::Stack::push(L, self.mesh.getTriangle(checkTriangle(L, self.mesh, 2)));
    return 1;
}

int MeshBvhBinding::vertexCount(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<ScriptedMesh>(L, 1).mesh.getVertexCount());
    return 1;
}

int MeshBvhBinding::triangleCount(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<ScriptedMesh>(L, 1).mesh.getTriangleCount());
    return 1;
}

int MeshBvhBinding::bounds(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<ScriptedMesh>(L, 1).mesh.getBounds());
    return 1;
}

int MeshBvhBinding::nodeCount(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<ScriptedMesh>(L, 1).mesh.getNodeCount());
    return 1;
}

int MeshBvhBinding::depth(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<ScriptedMesh>(L, 1).mesh.getDepth());
    return 1;
}

void MeshBvhBinding::addFunctions(lua_State* L) {
    const luaL_Reg functions[] = {
        {"newMeshBvh", &lua::Binding::native<&newMeshBvh>},
        {nullptr, nullptr},
    };
    luaL_setfuncs(L, functions, 0);
}

void MeshBvhBinding::install(lua_State* L) {
    lua::ClassBuilder<ScriptedMesh>(L).function("raycast", &lua::Binding::native<&raycast>).function("intersects", &lua::Binding::native<&intersects>).function("closestPoint", &lua::Binding::native<&closestPoint>).function("queryBox", &lua::Binding::native<&queryBox>).function("querySphere", &lua::Binding::native<&querySphere>).function("triangle", &lua::Binding::native<&triangle>).property("vertexCount", &vertexCount).property("triangleCount", &triangleCount).property("bounds", &bounds).property("nodeCount", &nodeCount).property("depth", &depth).install();
}

} // namespace haylen::lua::spatial3d
