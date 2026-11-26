#pragma once

#include <lua.hpp>

#include <array>
#include <memory>
#include <optional>
#include <string_view>

#include "haylen/3d/physics/Shape.hpp"
#include "haylen/3d/physics/World.hpp"
#include "haylen/core/EnumNames.hpp"
#include "haylen/lua/Type.hpp"

namespace haylen::lua {

template <> struct Type<haylen::physics3d::World> {
    static constexpr const char* name = "haylen.PhysicsWorld3D";
    using Storage = std::shared_ptr<haylen::physics3d::World>;
};

template <> struct Type<const haylen::physics3d::Shape> {
    static constexpr const char* name = "haylen.Shape3D";
    using Storage = std::shared_ptr<const haylen::physics3d::Shape>;
};

} // namespace haylen::lua

namespace haylen::core {

template <> struct EnumNames<haylen::physics3d::Shape::Kind> {
    static std::optional<haylen::physics3d::Shape::Kind> fromName(std::string_view name) {
        return haylen::physics3d::Shape::kindFromName(name);
    }
    static std::string_view name(haylen::physics3d::Shape::Kind value) {
        return haylen::physics3d::Shape::kindName(value);
    }
};

} // namespace haylen::core

namespace haylen::lua::physics3d {

// Installs `haylen.physics3d` with the `PhysicsWorld3D` class, the worlds in meters that the selected backend simulates, and the `Shape3D` class of their collision shapes.
class Physics3DBinding final {
  public:
    static void install(lua_State* L);

  private:
    static constexpr std::array<std::string_view, 1> kOwnerField{"owner"};

    static int newWorld(lua_State* L);
    static int getBackends(lua_State* L);
    static int open(lua_State* L);
};

} // namespace haylen::lua::physics3d
