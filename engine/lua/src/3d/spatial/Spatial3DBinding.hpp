#pragma once

#include <cstddef>

#include "haylen/math/Vec3.hpp"

struct lua_State;

namespace haylen::lua::spatial3d {

// Installs `haylen.spatial3d` with its spatial structures, the bounding volume hierarchy of meshes and the walk of the cells of a grid.
class Spatial3DBinding final {
  public:
    static void install(lua_State* L);

  private:
    static constexpr std::size_t kMaxTraversedCells = 65536;

    // Reads a cell size that is a number for cubic cells or a `Vec3`.
    [[nodiscard]] static haylen::math::Vec3 readCellSize(lua_State* L, int index);
    static int traverseGrid(lua_State* L);
    static int open(lua_State* L);
};

} // namespace haylen::lua::spatial3d
