#include "3d/spatial/Spatial3DBinding.hpp"

#include <lua.hpp>

#include <stdexcept>

#include "3d/spatial/MeshBvhBinding.hpp"
#include "3d/spatial/SpatialIndexBinding.hpp"
#include "haylen/3d/spatial/Cell.hpp"
#include "haylen/3d/spatial/GridRay.hpp"
#include "haylen/lua/Binding.hpp"
#include "haylen/lua/Stack.hpp"
#include "haylen/lua/TypeConverter.hpp"
#include "haylen/math/Ray3D.hpp"

namespace haylen::lua::spatial3d {

haylen::math::Vec3 Spatial3DBinding::readCellSize(lua_State* L, int index) {
    if (lua_type(L, index) == LUA_TNUMBER) {
        const auto side = lua::Stack::read<float>(L, index);
        return {side, side, side};
    }

    return lua::Stack::read<haylen::math::Vec3>(L, index);
}

// Lists the cells that the segment from one point to another crosses with `traverseGrid(from, to, cellSize)`, in order, each as `{x, y, z, distance, normal}` with the distance where the segment enters the cell and the normal of the face it enters through.
int Spatial3DBinding::traverseGrid(lua_State* L) {
    const haylen::math::Ray3D ray = haylen::math::Ray3D::between(lua::Stack::read<haylen::math::Vec3>(L, 1), lua::Stack::read<haylen::math::Vec3>(L, 2));
    const haylen::math::Vec3 cellSize = readCellSize(L, 3);
    lua_newtable(L);
    lua_Integer count = 0;
    // clang-format off
    haylen::spatial3d::GridRay::traverse(ray, cellSize, [L, &count](haylen::spatial3d::Cell cell, float distance, haylen::math::Vec3 normal) {
        if (static_cast<std::size_t>(count) == kMaxTraversedCells) {
            throw std::invalid_argument("A grid traversal lists at most 65536 cells.");
        }

        lua_createtable(L, 0, 5);
        lua_pushinteger(L, cell.x);
        lua_setfield(L, -2, "x");
        lua_pushinteger(L, cell.y);
        lua_setfield(L, -2, "y");
        lua_pushinteger(L, cell.z);
        lua_setfield(L, -2, "z");
        lua::Stack::push(L, distance);
        lua_setfield(L, -2, "distance");
        lua::Stack::push(L, normal);
        lua_setfield(L, -2, "normal");
        lua_rawseti(L, -2, ++count);
        return true;
    });
    // clang-format on
    return 1;
}

int Spatial3DBinding::open(lua_State* L) {
    const luaL_Reg functions[] = {
        {"traverseGrid", &lua::Binding::native<&traverseGrid>},
        {nullptr, nullptr},
    };
    lua::Binding::newModule(L, functions);
    SpatialIndexBinding::addFunctions(L);
    MeshBvhBinding::addFunctions(L);
    return 1;
}

void Spatial3DBinding::install(lua_State* L) {
    SpatialIndexBinding::install(L);
    MeshBvhBinding::install(L);
    lua::Binding::preload(L, "haylen.spatial3d", &open);
}

} // namespace haylen::lua::spatial3d
