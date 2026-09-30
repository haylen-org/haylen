#pragma once

#include <lua.hpp>

#include <array>
#include <cstddef>
#include <memory>
#include <optional>
#include <string_view>
#include <vector>

#include "2d/physics/ScriptedHandle.hpp"
#include "haylen/2d/physics/Body.hpp"
#include "haylen/2d/physics/Joint.hpp"
#include "haylen/2d/physics/Shape.hpp"
#include "haylen/2d/physics/World.hpp"
#include "haylen/lua/EnumNames.hpp"
#include "haylen/lua/Type.hpp"
#include "haylen/lua/Userdata.hpp"

namespace haylen::lua {

template <> struct Type<physics2d::World> {
    static constexpr const char* name = "haylen.PhysicsWorld";
    using Storage = std::shared_ptr<physics2d::World>;
};

template <> struct Type<physics2d::ScriptedHandle<physics2d::Body>> {
    static constexpr const char* name = "haylen.Body";
    using Storage = physics2d::ScriptedHandle<physics2d::Body>;
};

template <> struct Type<physics2d::ScriptedHandle<physics2d::Shape>> {
    static constexpr const char* name = "haylen.Shape";
    using Storage = physics2d::ScriptedHandle<physics2d::Shape>;
};

template <> struct Type<physics2d::ScriptedHandle<physics2d::Joint>> {
    static constexpr const char* name = "haylen.Joint";
    using Storage = physics2d::ScriptedHandle<physics2d::Joint>;
};

template <> struct EnumNames<physics2d::Body::Type> {
    static std::optional<physics2d::Body::Type> fromName(std::string_view name) {
        return physics2d::Body::typeFromName(name);
    }
    static std::string_view name(physics2d::Body::Type value) {
        return physics2d::Body::typeName(value);
    }
};

template <> struct EnumNames<physics2d::Shape::Kind> {
    static std::optional<physics2d::Shape::Kind> fromName(std::string_view name) {
        return physics2d::Shape::kindFromName(name);
    }
    static std::string_view name(physics2d::Shape::Kind value) {
        return physics2d::Shape::kindName(value);
    }
};

template <> struct EnumNames<physics2d::Joint::Type> {
    static std::optional<physics2d::Joint::Type> fromName(std::string_view name) {
        return physics2d::Joint::typeFromName(name);
    }
    static std::string_view name(physics2d::Joint::Type value) {
        return physics2d::Joint::typeName(value);
    }
};

} // namespace haylen::lua

namespace haylen::physics2d {

// Installs `haylen.physics2d` with the `PhysicsWorld`, `Body`, `Shape` and `Joint` classes, and the ropes, ragdolls, vehicles, terrains, explosions, fractures and fluids built on them.
class Physics2DLua final {
  public:
    static void install(lua_State* L);

    // Pushes a body, shape or joint handle that belongs to the Lua world object at `worldIndex`.
    template <typename Handle> static void push(lua_State* L, int worldIndex, Handle handle) {
        const int owner = lua_absindex(L, worldIndex);
        lua::Userdata::emplace<ScriptedHandle<Handle>>(L, ScriptedHandle<Handle>{lua::Userdata::checkShared<World>(L, owner), handle});
        lua_pushvalue(L, owner);
        lua_setiuservalue(L, -2, 1);
    }

    template <typename Handle> static void pushList(lua_State* L, int worldIndex, const std::vector<Handle>& handles) {
        const int owner = lua_absindex(L, worldIndex);
        lua_createtable(L, static_cast<int>(handles.size()), 0);
        for (std::size_t index = 0; index < handles.size(); ++index) {
            push(L, owner, handles[index]);
            lua_rawseti(L, -2, static_cast<lua_Integer>(index + 1));
        }
    }

  private:
    static constexpr std::array<std::string_view, 3> kWorldFields{"gravity", "pixelsPerMeter", "subSteps"};

    static int newWorld(lua_State* L);
    static int open(lua_State* L);
};

} // namespace haylen::physics2d
