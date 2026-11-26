#pragma once

#include <lua.hpp>

#include <array>
#include <memory>
#include <string_view>
#include <vector>

#include "haylen/3d/physics/Shape.hpp"
#include "haylen/lua/Table.hpp"

namespace haylen::lua::physics3d {

// Installs the `Shape3D` class of `haylen.physics3d` and the functions of the module that make shapes, with an asynchronous version of each heavy one that builds on the task pool. Vertices, triangles and samples count from one like a Lua array.
class ShapeBinding final {
  public:
    static void install(lua_State* L);

    // Sets the functions that make shapes on the module table at the top of the stack.
    static void addFunctions(lua_State* L);

    // Pushes a shape as a `Shape3D`.
    static void push(lua_State* L, std::shared_ptr<const haylen::physics3d::Shape> shape);

    // Returns the shape of the `Shape3D` at the stack index.
    [[nodiscard]] static const std::shared_ptr<const haylen::physics3d::Shape>& check(lua_State* L, int index);

  private:
    static constexpr std::array<std::string_view, 1> kDensityFields{"density"};
    static constexpr std::array<std::string_view, 2> kRoundedFields{"density", "convexRadius"};
    static constexpr std::array<std::string_view, 3> kHeightFieldFields{"scale", "offset", "holes"};
    static constexpr std::array<std::string_view, 1> kPlaneFields{"halfExtent"};
    static constexpr std::array<std::string_view, 1> kCompoundFields{"mutable"};
    static constexpr std::array<std::string_view, 3> kChildFields{"shape", "position", "rotation"};

    // Reads the density and, for a shape with edges to round, the convex radius of the options at the stack index into the settings of the geometry.
    template <typename Geometry> [[nodiscard]] static haylen::physics3d::Shape::Settings readSettings(lua_State* L, int options, Geometry geometry);

    [[nodiscard]] static haylen::physics3d::Shape::Settings readHull(lua_State* L);
    [[nodiscard]] static haylen::physics3d::Shape::Settings readMesh(lua_State* L);
    [[nodiscard]] static haylen::physics3d::Shape::Settings readHeightField(lua_State* L);
    [[nodiscard]] static std::vector<haylen::physics3d::Shape::Child> readChildren(lua_State* L, int index);
    [[nodiscard]] static std::vector<float> readHeights(lua_State* L, int index);
    [[nodiscard]] static std::vector<std::uint32_t> readHoles(lua_State* L, int index, std::size_t samples);

    // Makes the shape now and pushes it, or makes it on the task pool and pushes a promise of it.
    static int make(lua_State* L, const haylen::physics3d::Shape::Settings& settings);
    static int makeAsync(lua_State* L, haylen::physics3d::Shape::Settings settings);

    static int newSphere(lua_State* L);
    static int newBox(lua_State* L);
    static int newCapsule(lua_State* L);
    static int newTaperedCapsule(lua_State* L);
    static int newCylinder(lua_State* L);
    static int newTaperedCylinder(lua_State* L);
    static int newConvexHull(lua_State* L);
    static int newConvexHullAsync(lua_State* L);
    static int newMesh(lua_State* L);
    static int newMeshAsync(lua_State* L);
    static int newHeightField(lua_State* L);
    static int newHeightFieldAsync(lua_State* L);
    static int newPlane(lua_State* L);
    static int newCompound(lua_State* L);
    static int newScaled(lua_State* L);
    static int newOffsetCenterOfMass(lua_State* L);

    static int getKind(lua_State* L);
    static int getBackend(lua_State* L);
    static int getBounds(lua_State* L);
    static int getCenterOfMass(lua_State* L);
    static int getVolume(lua_State* L);
    static int getMass(lua_State* L);
    static int isSolid(lua_State* L);
};

} // namespace haylen::lua::physics3d
