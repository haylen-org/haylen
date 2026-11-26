#pragma once

#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "haylen/3d/navigation/SourceGeometry.hpp"
#include "haylen/lua/Type.hpp"

struct lua_State;

namespace haylen::lua {

template <> struct Type<haylen::navigation3d::SourceGeometry> {
    static constexpr const char* name = "haylen.SourceGeometry";
    using Storage = std::shared_ptr<haylen::navigation3d::SourceGeometry>;
};

} // namespace haylen::lua

namespace haylen::lua::navigation3d {

// Installs the `SourceGeometry` class, the world that a navigation mesh bakes from, with its meshes, shapes, heightfields, volumes and links.
class GeometryBinding final {
  public:
    static constexpr std::array<std::string_view, 2> kMeshFields{"transform", "area"};
    static constexpr std::array<std::string_view, 1> kAreaField{"area"};
    static constexpr std::array<std::string_view, 3> kHeightfieldFields{"origin", "spacing", "area"};

    static void install(lua_State* L);

    // Creates an empty geometry with `newGeometry()`.
    static int create(lua_State* L);

  private:
    [[nodiscard]] static haylen::navigation3d::SourceGeometry& check(lua_State* L);
    [[nodiscard]] static std::string readArea(lua_State* L, int index);
    [[nodiscard]] static std::vector<haylen::math::Vec3> readPoints(lua_State* L, int index);

    static int addMesh(lua_State* L);
    static int addBox(lua_State* L);
    static int addSphere(lua_State* L);
    static int addCapsule(lua_State* L);
    static int addCylinder(lua_State* L);
    static int addHeightfield(lua_State* L);
    static int addVolume(lua_State* L);
    static int addLink(lua_State* L);
    static int areas(lua_State* L);
    static int bounds(lua_State* L);
    static int triangleCount(lua_State* L);
};

} // namespace haylen::lua::navigation3d
