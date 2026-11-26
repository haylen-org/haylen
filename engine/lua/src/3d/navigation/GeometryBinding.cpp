#include "3d/navigation/GeometryBinding.hpp"

#include <lua.hpp>

#include <span>
#include <utility>

#include "3d/navigation/NavigationOptions.hpp"
#include "3d/spatial/MeshBvhBinding.hpp"
#include "core/FloatBufferBinding.hpp"
#include "haylen/core/FloatBuffer.hpp"
#include "haylen/lua/Binding.hpp"
#include "haylen/lua/ClassBuilder.hpp"
#include "haylen/lua/Stack.hpp"
#include "haylen/lua/Table.hpp"
#include "haylen/lua/TypeConverter.hpp"
#include "haylen/lua/Userdata.hpp"

namespace haylen::lua::navigation3d {

haylen::navigation3d::SourceGeometry& GeometryBinding::check(lua_State* L) {
    return lua::Userdata::check<haylen::navigation3d::SourceGeometry>(L, 1);
}

std::string GeometryBinding::readArea(lua_State* L, int index) {
    std::string area(haylen::navigation3d::SourceGeometry::kGround);
    if (!lua_isnoneornil(L, index)) {
        luaL_checktype(L, index, LUA_TTABLE);
        lua::Table::checkFields(L, index, {kAreaField});
        lua::Table::readField(L, index, "area", area);
    }

    return area;
}

std::vector<haylen::math::Vec3> GeometryBinding::readPoints(lua_State* L, int index) {
    const std::vector<float> values = lua::spatial3d::MeshBvhBinding::readPositions(L, index);
    std::vector<haylen::math::Vec3> points;
    points.reserve(values.size() / 3);
    for (std::size_t point = 0; point + 2 < values.size(); point += 3) {
        points.push_back({values[point], values[point + 1], values[point + 2]});
    }

    return points;
}

int GeometryBinding::create(lua_State* L) {
    lua::Userdata::emplace<haylen::navigation3d::SourceGeometry>(L, std::make_shared<haylen::navigation3d::SourceGeometry>());
    return 1;
}

// Adds a mesh with `addMesh(positions[, indices][, options])`, whose options are `transform` and `area`.
int GeometryBinding::addMesh(lua_State* L) {
    haylen::navigation3d::SourceGeometry& self = check(L);
    const std::vector<haylen::math::Vec3> positions = readPoints(L, 2);
    const std::vector<std::uint32_t> indices = lua::spatial3d::MeshBvhBinding::readIndices(L, 3, positions.size());
    haylen::math::Mat4 transform = haylen::math::Mat4::identity();
    std::string area(haylen::navigation3d::SourceGeometry::kGround);
    if (!lua_isnoneornil(L, 4)) {
        luaL_checktype(L, 4, LUA_TTABLE);
        lua::Table::checkFields(L, 4, {kMeshFields});
        lua::Table::readField(L, 4, "transform", transform);
        lua::Table::readField(L, 4, "area", area);
    }

    self.addMesh(positions, indices, transform, area);
    return 0;
}

int GeometryBinding::addBox(lua_State* L) {
    check(L).addBox(lua::Stack::read<haylen::math::Obb>(L, 2), readArea(L, 3));
    return 0;
}

int GeometryBinding::addSphere(lua_State* L) {
    check(L).addSphere(lua::Stack::read<haylen::math::Sphere>(L, 2), readArea(L, 3));
    return 0;
}

int GeometryBinding::addCapsule(lua_State* L) {
    check(L).addCapsule(lua::Stack::read<haylen::math::Capsule>(L, 2), readArea(L, 3));
    return 0;
}

// Adds a cylinder between the centers of its caps with `addCylinder(start, end, radius[, options])`.
int GeometryBinding::addCylinder(lua_State* L) {
    check(L).addCylinder(lua::Stack::read<haylen::math::Vec3>(L, 2), lua::Stack::read<haylen::math::Vec3>(L, 3), lua::Stack::read<float>(L, 4), readArea(L, 5));
    return 0;
}

// Adds terrain with `addHeightfield(heights, columns, rows[, options])`, whose heights are a float buffer or a list of numbers, row by row, and whose options are `origin`, `spacing` and `area`.
int GeometryBinding::addHeightfield(lua_State* L) {
    haylen::navigation3d::SourceGeometry& self = check(L);
    haylen::navigation3d::SourceGeometry::Heightfield terrain;
    if (const haylen::core::FloatBuffer* buffer = lua::Userdata::test<haylen::core::FloatBuffer>(L, 2)) {
        const std::span<const float> values = buffer->getValues();
        terrain.heights.assign(values.begin(), values.end());
    } else {
        terrain.heights = lua::Stack::read<std::vector<float>>(L, 2);
    }

    terrain.columns = lua::Stack::read<std::size_t>(L, 3);
    terrain.rows = lua::Stack::read<std::size_t>(L, 4);
    std::string area(haylen::navigation3d::SourceGeometry::kGround);
    if (!lua_isnoneornil(L, 5)) {
        luaL_checktype(L, 5, LUA_TTABLE);
        lua::Table::checkFields(L, 5, {kHeightfieldFields});
        lua::Table::readField(L, 5, "origin", terrain.origin);
        lua::Table::readField(L, 5, "spacing", terrain.spacing);
        lua::Table::readField(L, 5, "area", area);
    }

    self.addHeightfield(terrain, area);
    return 0;
}

// Marks an area with `addVolume(outline, bottom, top[, options])`, whose outline is a list of points on the ground.
int GeometryBinding::addVolume(lua_State* L) {
    haylen::navigation3d::SourceGeometry& self = check(L);
    haylen::navigation3d::SourceGeometry::Volume volume{.outline = lua::Stack::read<std::vector<haylen::math::Vec3>>(L, 2), .bottom = lua::Stack::read<float>(L, 3), .top = lua::Stack::read<float>(L, 4), .area = readArea(L, 5)};
    self.addVolume(volume);
    return 0;
}

// Adds a link with `addLink(start, end[, options])`, whose options are `radius`, `bidirectional`, `kind` and `area`.
int GeometryBinding::addLink(lua_State* L) {
    check(L).addLink(NavigationOptions::readLink(L, 2, 3, 4));
    return 0;
}

int GeometryBinding::areas(lua_State* L) {
    lua::Stack::push(L, check(L).getAreas());
    return 1;
}

int GeometryBinding::bounds(lua_State* L) {
    const haylen::navigation3d::SourceGeometry& self = check(L);
    if (self.isEmpty()) {
        lua_pushnil(L);
        return 1;
    }

    lua::Stack::push(L, self.getBounds());
    return 1;
}

int GeometryBinding::triangleCount(lua_State* L) {
    lua::Stack::push(L, check(L).getTriangleCount());
    return 1;
}

void GeometryBinding::install(lua_State* L) {
    lua::ClassBuilder<haylen::navigation3d::SourceGeometry>(L).function("addMesh", &lua::Binding::native<&addMesh>).function("addBox", &lua::Binding::native<&addBox>).function("addSphere", &lua::Binding::native<&addSphere>).function("addCapsule", &lua::Binding::native<&addCapsule>).function("addCylinder", &lua::Binding::native<&addCylinder>).function("addHeightfield", &lua::Binding::native<&addHeightfield>).function("addVolume", &lua::Binding::native<&addVolume>).function("addLink", &lua::Binding::native<&addLink>).property("areas", &lua::Binding::native<&areas>).property("bounds", &lua::Binding::native<&bounds>).property("triangleCount", &lua::Binding::native<&triangleCount>).install();
}

} // namespace haylen::lua::navigation3d
