#pragma once

#include <lua.hpp>

#include <cstddef>
#include <cstdint>
#include <vector>

#include "3d/spatial/ScriptedMesh.hpp"
#include "haylen/3d/spatial/MeshBvh.hpp"
#include "haylen/lua/Type.hpp"

namespace haylen::lua {

template <> struct Type<lua::spatial3d::ScriptedMesh> {
    static constexpr const char* name = "haylen.MeshBvh";
    using Storage = lua::spatial3d::ScriptedMesh;
};

} // namespace haylen::lua

namespace haylen::lua::spatial3d {

// Installs the `MeshBvh` class of `haylen.spatial3d`, whose triangles and vertices count from one like a Lua array.
class MeshBvhBinding final {
  public:
    static void install(lua_State* L);

    // Sets `newMeshBvh` on the module table at the top of the stack.
    static void addFunctions(lua_State* L);

    // Reads the positions at the stack index, from a float buffer of three numbers for each vertex or a list of `Vec3` values, which the collision meshes of 3D physics and the geometry of navigation meshes read the same way.
    [[nodiscard]] static std::vector<float> readPositions(lua_State* L, int index);

    // Reads the optional list of one-based vertices at the stack index, three for each triangle, and returns them counted from zero.
    [[nodiscard]] static std::vector<std::uint32_t> readIndices(lua_State* L, int index, std::size_t vertexCount);

  private:
    // Creates a hierarchy with `newMeshBvh(positions[, indices])`, from a float buffer or a list of `Vec3` values and an optional list of vertices, three for each triangle.
    static int newMeshBvh(lua_State* L);

    // Reads the one-based triangle at the stack index and returns it counted from zero.
    [[nodiscard]] static std::uint32_t checkTriangle(lua_State* L, const haylen::spatial3d::MeshBvh& mesh, int index);
    static void pushTriangles(lua_State* L, const std::vector<std::uint32_t>& triangles, int results);

    static int raycast(lua_State* L);
    static int intersects(lua_State* L);
    static int closestPoint(lua_State* L);
    static int queryBox(lua_State* L);
    static int querySphere(lua_State* L);
    static int triangle(lua_State* L);
    static int vertexCount(lua_State* L);
    static int triangleCount(lua_State* L);
    static int bounds(lua_State* L);
    static int nodeCount(lua_State* L);
    static int depth(lua_State* L);
};

} // namespace haylen::lua::spatial3d
