#pragma once

#include <array>
#include <cstdint>
#include <memory>
#include <string_view>
#include <vector>

#include "haylen/2d/graphics/Mesh.hpp"
#include "haylen/lua/Type.hpp"

struct lua_State;

namespace haylen::lua {

template <> struct Type<graphics2d::Mesh> {
    static constexpr const char* name = "haylen.Mesh";
    using Storage = std::shared_ptr<graphics2d::Mesh>;
};

} // namespace haylen::lua

namespace haylen::graphics2d {

// Installs the `Mesh` class of `haylen.graphics2d`, whose vertices and indices count from one, and reads the vertex and index lists that `graphics2d.drawMesh` takes too.
class MeshLua final {
  public:
    static constexpr std::array<std::string_view, 5> kVertexFields{"x", "y", "u", "v", "color"};

    static void install(lua_State* L);

    // Creates a mesh with `graphics2d.newMesh(texture or nil, vertices, indices)`.
    static int create(lua_State* L);

    // Creates a grid with `graphics2d.newGridMesh(texture or nil, columns, rows, {x, y, width, height, source})`.
    static int createGrid(lua_State* L);

    static void readVertex(lua_State* L, int index, MeshVertex& vertex);
    static void readVertices(lua_State* L, int index, std::vector<MeshVertex>& vertices);
    static void readIndices(lua_State* L, int index, std::vector<std::uint32_t>& indices);

  private:
    static constexpr std::array<std::string_view, 5> kGridFields{"x", "y", "width", "height", "source"};

    static int setPositions(lua_State* L);
    static int readPositions(lua_State* L);
    static int setVertex(lua_State* L);
    static int vertexAt(lua_State* L);
    static int setIndices(lua_State* L);
    static int draw(lua_State* L);
    static int vertexCount(lua_State* L);
    static int indexCount(lua_State* L);
    static int bounds(lua_State* L);
    static int getTexture(lua_State* L);
    static int setTexture(lua_State* L);

    // Returns the vertex the argument at the index names, counted from one.
    [[nodiscard]] static std::size_t checkVertex(lua_State* L, int index, const Mesh& mesh);
};

} // namespace haylen::graphics2d
