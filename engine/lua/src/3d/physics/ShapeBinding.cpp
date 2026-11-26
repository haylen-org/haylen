#include "3d/physics/ShapeBinding.hpp"

#include <cstddef>
#include <cstdint>
#include <format>
#include <span>
#include <stdexcept>
#include <utility>

#include "3d/physics/Physics3DBinding.hpp"
#include "3d/spatial/MeshBvhBinding.hpp"
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
#include "haylen/plugins/Physics3DPlugin.hpp"

namespace haylen::lua::physics3d {

using haylen::physics3d::Shape;

void ShapeBinding::push(lua_State* L, std::shared_ptr<const Shape> shape) {
    lua::Userdata::emplace<const Shape>(L, std::move(shape));
}

const std::shared_ptr<const Shape>& ShapeBinding::check(lua_State* L, int index) {
    return lua::Userdata::checkShared<const Shape>(L, index);
}

template <typename Geometry> Shape::Settings ShapeBinding::readSettings(lua_State* L, int options, Geometry geometry) {
    Shape::Settings settings;
    if (!lua_isnoneornil(L, options)) {
        luaL_checktype(L, options, LUA_TTABLE);
        if constexpr (requires { geometry.convexRadius; }) {
            lua::Table::checkFields(L, options, {kRoundedFields});
            lua::Table::readField(L, options, "convexRadius", geometry.convexRadius);
        } else {
            lua::Table::checkFields(L, options, {kDensityFields});
        }

        lua::Table::readField(L, options, "density", settings.density);
    }

    settings.geometry = std::move(geometry);
    return settings;
}

int ShapeBinding::make(lua_State* L, const Shape::Settings& settings) {
    push(L, lua::Runtime::getEngine(L).getPlugin<haylen::plugins::Physics3DPlugin>().createShape(settings));
    return 1;
}

// A shape that fails to build rejects the promise with the sentence that says why.
int ShapeBinding::makeAsync(lua_State* L, Shape::Settings settings) {
    haylen::core::Engine& engine = lua::Runtime::getEngine(L);
    const lua::Promise promise(engine);
    // clang-format off
    engine.getPlugin<haylen::plugins::Physics3DPlugin>().createShapeAsync(std::move(settings), [promise](haylen::core::JobSystem::Result<std::shared_ptr<const Shape>> result) {
        promise.settle(std::move(result), [](lua_State* state, const std::shared_ptr<const Shape>& shape) { push(state, shape); });
    });
    // clang-format on
    promise.push(L);
    return 1;
}

// Makes a sphere with `newSphere(radius[, {density}])`.
int ShapeBinding::newSphere(lua_State* L) {
    return make(L, readSettings(L, 2, Shape::Sphere{.radius = lua::Stack::read<float>(L, 1)}));
}

// Makes a box with `newBox(size[, {density, convexRadius}])`, whose size is its full width, height and depth.
int ShapeBinding::newBox(lua_State* L) {
    return make(L, readSettings(L, 2, Shape::Box{.size = lua::Stack::read<haylen::math::Vec3>(L, 1)}));
}

// Makes a capsule standing along `y` with `newCapsule(height, radius[, {density}])`.
int ShapeBinding::newCapsule(lua_State* L) {
    return make(L, readSettings(L, 3, Shape::Capsule{.height = lua::Stack::read<float>(L, 1), .radius = lua::Stack::read<float>(L, 2)}));
}

// Makes a tapered capsule with `newTaperedCapsule(height, topRadius, bottomRadius[, {density}])`.
int ShapeBinding::newTaperedCapsule(lua_State* L) {
    return make(L, readSettings(L, 4, Shape::TaperedCapsule{.height = lua::Stack::read<float>(L, 1), .topRadius = lua::Stack::read<float>(L, 2), .bottomRadius = lua::Stack::read<float>(L, 3)}));
}

// Makes a cylinder standing along `y` with `newCylinder(height, radius[, {density, convexRadius}])`.
int ShapeBinding::newCylinder(lua_State* L) {
    return make(L, readSettings(L, 3, Shape::Cylinder{.height = lua::Stack::read<float>(L, 1), .radius = lua::Stack::read<float>(L, 2)}));
}

// Makes a tapered cylinder with `newTaperedCylinder(height, topRadius, bottomRadius[, {density, convexRadius}])`.
int ShapeBinding::newTaperedCylinder(lua_State* L) {
    return make(L, readSettings(L, 4, Shape::TaperedCylinder{.height = lua::Stack::read<float>(L, 1), .topRadius = lua::Stack::read<float>(L, 2), .bottomRadius = lua::Stack::read<float>(L, 3)}));
}

Shape::Settings ShapeBinding::readHull(lua_State* L) {
    const std::vector<float> positions = lua::spatial3d::MeshBvhBinding::readPositions(L, 1);
    if (positions.size() % 3 != 0) {
        throw std::invalid_argument("The points of a convex hull take three numbers for each point.");
    }

    Shape::ConvexHull hull;
    hull.points.reserve(positions.size() / 3);
    for (std::size_t base = 0; base < positions.size(); base += 3) {
        hull.points.push_back({positions[base], positions[base + 1], positions[base + 2]});
    }

    return readSettings(L, 2, std::move(hull));
}

// Makes the hull of the points with `newConvexHull(points[, {density, convexRadius}])`, from a float buffer of three numbers for each point or a list of `Vec3` values.
int ShapeBinding::newConvexHull(lua_State* L) {
    return make(L, readHull(L));
}

int ShapeBinding::newConvexHullAsync(lua_State* L) {
    return makeAsync(L, readHull(L));
}

// Takes a `MeshBvh`, or the positions and the optional indices that `newMeshBvh` takes, with no options, since a mesh has no mass.
Shape::Settings ShapeBinding::readMesh(lua_State* L) {
    if (const lua::spatial3d::ScriptedMesh* scripted = lua::Userdata::test<lua::spatial3d::ScriptedMesh>(L, 1)) {
        return {.geometry = Shape::Mesh::fromBvh(scripted->mesh)};
    }

    Shape::Mesh mesh;
    mesh.positions = lua::spatial3d::MeshBvhBinding::readPositions(L, 1);
    mesh.indices = lua::spatial3d::MeshBvhBinding::readIndices(L, 2, mesh.positions.size() / 3);
    return {.geometry = std::move(mesh)};
}

// Makes a triangle mesh with `newMesh(positions[, indices])` or `newMesh(meshBvh)`.
int ShapeBinding::newMesh(lua_State* L) {
    return make(L, readMesh(L));
}

int ShapeBinding::newMeshAsync(lua_State* L) {
    return makeAsync(L, readMesh(L));
}

std::vector<float> ShapeBinding::readHeights(lua_State* L, int index) {
    if (const haylen::core::FloatBuffer* buffer = lua::Userdata::test<haylen::core::FloatBuffer>(L, index)) {
        const std::span<const float> values = buffer->getValues();
        return {values.begin(), values.end()};
    }

    luaL_checktype(L, index, LUA_TTABLE);
    const auto count = static_cast<std::size_t>(lua_rawlen(L, index));
    std::vector<float> heights;
    heights.reserve(count);
    for (std::size_t item = 1; item <= count; ++item) {
        lua_rawgeti(L, index, static_cast<lua_Integer>(item));
        int number = 0;
        const lua_Number height = lua_tonumberx(L, -1, &number);
        lua_pop(L, 1);
        if (number == 0) {
            throw std::invalid_argument(std::format("Item {} of the heights of a heightfield must be a number.", item));
        }

        heights.push_back(static_cast<float>(height));
    }

    return heights;
}

std::vector<std::uint32_t> ShapeBinding::readHoles(lua_State* L, int index, std::size_t samples) {
    luaL_checktype(L, index, LUA_TTABLE);
    const auto count = static_cast<std::size_t>(lua_rawlen(L, index));
    std::vector<std::uint32_t> holes;
    holes.reserve(count);
    for (std::size_t item = 1; item <= count; ++item) {
        lua_rawgeti(L, index, static_cast<lua_Integer>(item));
        int whole = 0;
        const lua_Integer sample = lua_tointegerx(L, -1, &whole);
        lua_pop(L, 1);
        if (whole == 0 || sample < 1 || static_cast<std::uint64_t>(sample) > samples) {
            throw std::invalid_argument(std::format("Item {} of the holes of a heightfield must name a sample from 1 to {}.", item, samples));
        }

        holes.push_back(static_cast<std::uint32_t>(sample - 1));
    }

    return holes;
}

Shape::Settings ShapeBinding::readHeightField(lua_State* L) {
    Shape::HeightField field;
    field.heights = readHeights(L, 1);
    field.sampleCount = lua::Stack::read<int>(L, 2);
    if (!lua_isnoneornil(L, 3)) {
        luaL_checktype(L, 3, LUA_TTABLE);
        lua::Table::checkFields(L, 3, {kHeightFieldFields});
        lua::Table::readField(L, 3, "scale", field.scale);
        lua::Table::readField(L, 3, "offset", field.offset);
        if (lua_getfield(L, 3, "holes") != LUA_TNIL) {
            field.holes = readHoles(L, lua_gettop(L), field.heights.size());
        }

        lua_pop(L, 1);
    }

    return {.geometry = std::move(field)};
}

// Makes a terrain with `newHeightField(heights, sampleCount[, {scale, offset, holes}])`, whose heights fill `sampleCount` rows of `sampleCount` samples along `x`, from a float buffer or a list of numbers, and whose holes list the samples without collision.
int ShapeBinding::newHeightField(lua_State* L) {
    return make(L, readHeightField(L));
}

int ShapeBinding::newHeightFieldAsync(lua_State* L) {
    return makeAsync(L, readHeightField(L));
}

// Makes the half space under a plane with `newPlane(normal, distance[, {halfExtent}])`, where `distance` is how far the plane lies from the origin along its normal.
int ShapeBinding::newPlane(lua_State* L) {
    Shape::Plane plane{.normal = lua::Stack::read<haylen::math::Vec3>(L, 1), .distance = lua::Stack::read<float>(L, 2)};
    if (!lua_isnoneornil(L, 3)) {
        luaL_checktype(L, 3, LUA_TTABLE);
        lua::Table::checkFields(L, 3, {kPlaneFields});
        lua::Table::readField(L, 3, "halfExtent", plane.halfExtent);
    }

    return make(L, {.geometry = plane});
}

std::vector<Shape::Child> ShapeBinding::readChildren(lua_State* L, int index) {
    luaL_checktype(L, index, LUA_TTABLE);
    const auto count = static_cast<std::size_t>(lua_rawlen(L, index));
    std::vector<Shape::Child> children;
    children.reserve(count);
    for (std::size_t item = 1; item <= count; ++item) {
        lua_rawgeti(L, index, static_cast<lua_Integer>(item));
        lua::Table::requireItemType(L, "children", static_cast<lua_Integer>(item), LUA_TTABLE);
        const int entry = lua_gettop(L);
        lua::Table::checkFields(L, entry, {kChildFields});
        Shape::Child child;
        lua_getfield(L, entry, "shape");
        child.shape = check(L, -1);
        lua_pop(L, 1);
        lua::Table::readField(L, entry, "position", child.position);
        lua::Table::readField(L, entry, "rotation", child.rotation);
        lua_pop(L, 1);
        children.push_back(std::move(child));
    }

    return children;
}

// Makes a compound with `newCompound(children[, {mutable}])`, whose children are tables `{shape, position, rotation}`. A mutable compound gives each body that takes it a copy whose children the body changes.
int ShapeBinding::newCompound(lua_State* L) {
    std::vector<Shape::Child> children = readChildren(L, 1);
    bool isMutable = false;
    if (!lua_isnoneornil(L, 2)) {
        luaL_checktype(L, 2, LUA_TTABLE);
        lua::Table::checkFields(L, 2, {kCompoundFields});
        lua::Table::readField(L, 2, "mutable", isMutable);
    }

    if (isMutable) {
        return make(L, {.geometry = Shape::MutableCompound{.children = std::move(children)}});
    }

    return make(L, {.geometry = Shape::StaticCompound{.children = std::move(children)}});
}

// Scales a shape with `newScaled(shape, scale)`, whose scale is a `Vec3` or one number for every axis.
int ShapeBinding::newScaled(lua_State* L) {
    const std::shared_ptr<const Shape>& shape = check(L, 1);
    haylen::math::Vec3 scale{};
    if (lua_type(L, 2) == LUA_TNUMBER) {
        const float uniform = lua::Stack::read<float>(L, 2);
        scale = {uniform, uniform, uniform};
    } else {
        scale = lua::Stack::read<haylen::math::Vec3>(L, 2);
    }

    return make(L, {.geometry = Shape::Scaled{.shape = shape, .scale = scale}});
}

// Moves the center of mass of a shape with `newOffsetCenterOfMass(shape, offset)`.
int ShapeBinding::newOffsetCenterOfMass(lua_State* L) {
    return make(L, {.geometry = Shape::OffsetCenterOfMass{.shape = check(L, 1), .offset = lua::Stack::read<haylen::math::Vec3>(L, 2)}});
}

int ShapeBinding::getKind(lua_State* L) {
    lua::Stack::push(L, check(L, 1)->getKind());
    return 1;
}

int ShapeBinding::getBackend(lua_State* L) {
    lua::Stack::push(L, check(L, 1)->getBackendName());
    return 1;
}

int ShapeBinding::getBounds(lua_State* L) {
    lua::Stack::push(L, check(L, 1)->getBounds());
    return 1;
}

int ShapeBinding::getCenterOfMass(lua_State* L) {
    lua::Stack::push(L, check(L, 1)->getCenterOfMass());
    return 1;
}

int ShapeBinding::getVolume(lua_State* L) {
    lua::Stack::push(L, check(L, 1)->getVolume());
    return 1;
}

int ShapeBinding::getMass(lua_State* L) {
    lua::Stack::push(L, check(L, 1)->getMass());
    return 1;
}

int ShapeBinding::isSolid(lua_State* L) {
    lua::Stack::push(L, check(L, 1)->isSolid());
    return 1;
}

void ShapeBinding::addFunctions(lua_State* L) {
    const luaL_Reg functions[] = {
        {"newSphere", &lua::Binding::native<&newSphere>}, {"newBox", &lua::Binding::native<&newBox>}, {"newCapsule", &lua::Binding::native<&newCapsule>}, {"newTaperedCapsule", &lua::Binding::native<&newTaperedCapsule>}, {"newCylinder", &lua::Binding::native<&newCylinder>}, {"newTaperedCylinder", &lua::Binding::native<&newTaperedCylinder>}, {"newConvexHull", &lua::Binding::native<&newConvexHull>}, {"newConvexHullAsync", &lua::Binding::native<&newConvexHullAsync>}, {"newMesh", &lua::Binding::native<&newMesh>}, {"newMeshAsync", &lua::Binding::native<&newMeshAsync>}, {"newHeightField", &lua::Binding::native<&newHeightField>}, {"newHeightFieldAsync", &lua::Binding::native<&newHeightFieldAsync>}, {"newPlane", &lua::Binding::native<&newPlane>}, {"newCompound", &lua::Binding::native<&newCompound>}, {"newScaled", &lua::Binding::native<&newScaled>}, {"newOffsetCenterOfMass", &lua::Binding::native<&newOffsetCenterOfMass>}, {nullptr, nullptr},
    };
    luaL_setfuncs(L, functions, 0);
}

void ShapeBinding::install(lua_State* L) {
    lua::ClassBuilder<const Shape>(L).property("kind", &getKind).property("backend", &getBackend).property("bounds", &getBounds).property("centerOfMass", &getCenterOfMass).property("volume", &getVolume).property("mass", &getMass).property("solid", &isSolid).meta("__eq", &lua::Userdata::equal<const Shape>).install();
}

} // namespace haylen::lua::physics3d
