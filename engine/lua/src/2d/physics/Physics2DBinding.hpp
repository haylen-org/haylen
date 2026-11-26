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
#include "haylen/2d/physics/ContactManifold.hpp"
#include "haylen/2d/physics/Joint.hpp"
#include "haylen/2d/physics/Shape.hpp"
#include "haylen/2d/physics/World.hpp"
#include "haylen/core/EnumNames.hpp"
#include "haylen/lua/Type.hpp"
#include "haylen/lua/Userdata.hpp"

namespace haylen::lua {

template <> struct Type<haylen::physics2d::World> {
    static constexpr const char* name = "haylen.PhysicsWorld2D";
    using Storage = std::shared_ptr<haylen::physics2d::World>;
};

template <> struct Type<lua::physics2d::ScriptedHandle<haylen::physics2d::Body>> {
    static constexpr const char* name = "haylen.Body2D";
    using Storage = lua::physics2d::ScriptedHandle<haylen::physics2d::Body>;
};

template <> struct Type<lua::physics2d::ScriptedHandle<haylen::physics2d::Shape>> {
    static constexpr const char* name = "haylen.Shape2D";
    using Storage = lua::physics2d::ScriptedHandle<haylen::physics2d::Shape>;
};

template <> struct Type<lua::physics2d::ScriptedHandle<haylen::physics2d::Joint>> {
    static constexpr const char* name = "haylen.Joint2D";
    using Storage = lua::physics2d::ScriptedHandle<haylen::physics2d::Joint>;
};

} // namespace haylen::lua

namespace haylen::core {

template <> struct EnumNames<haylen::physics2d::Body::Type> {
    static std::optional<haylen::physics2d::Body::Type> fromName(std::string_view name) {
        return haylen::physics2d::Body::typeFromName(name);
    }
    static std::string_view name(haylen::physics2d::Body::Type value) {
        return haylen::physics2d::Body::typeName(value);
    }
};

template <> struct EnumNames<haylen::physics2d::Shape::Kind> {
    static std::optional<haylen::physics2d::Shape::Kind> fromName(std::string_view name) {
        return haylen::physics2d::Shape::kindFromName(name);
    }
    static std::string_view name(haylen::physics2d::Shape::Kind value) {
        return haylen::physics2d::Shape::kindName(value);
    }
};

template <> struct EnumNames<haylen::physics2d::Joint::Type> {
    static std::optional<haylen::physics2d::Joint::Type> fromName(std::string_view name) {
        return haylen::physics2d::Joint::typeFromName(name);
    }
    static std::string_view name(haylen::physics2d::Joint::Type value) {
        return haylen::physics2d::Joint::typeName(value);
    }
};

} // namespace haylen::core

namespace haylen::lua::physics2d {

// Installs `haylen.physics2d` with the `PhysicsWorld2D`, `Body2D`, `Shape2D` and `Joint2D` classes, and the ropes, ragdolls, vehicles, movers, grabbers, force fields, terrains, explosions, fractures and fluids built on them.
class Physics2DBinding final {
  public:
    static void install(lua_State* L);

    // Pushes a body, shape or joint handle that belongs to the Lua world object at `worldIndex`.
    // Sets the fields of a contact manifold on the table on top of the stack: `x`, `y`, `normalX`, `normalY`, `speed`, `relativeVelocityX`, `relativeVelocityY`, `force` and `points`, a list of `{x, y, separation, normalImpulse, tangentImpulse, approachSpeed}`.
    static void setManifold(lua_State* L, const haylen::physics2d::ContactManifold& manifold);

    template <typename Handle> static void push(lua_State* L, int worldIndex, Handle handle) {
        const int owner = lua_absindex(L, worldIndex);
        lua::Userdata::emplace<ScriptedHandle<Handle>>(L, ScriptedHandle<Handle>{lua::Userdata::checkShared<haylen::physics2d::World>(L, owner), handle});
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
    static constexpr std::array<std::string_view, 14> kWorldFields{"gravity", "pixelsPerMeter", "subSteps", "threads", "continuous", "sleepEnabled", "interpolate", "contactHertz", "contactDampingRatio", "contactPushSpeed", "maxSpeed", "restitutionThreshold", "hitThreshold", "owner"};

    // Worlds step on at most this many threads unless their options ask for more, because the solver gains little from more workers and slow cores hold it back.
    static constexpr int kDefaultThreads = 4;

    static int newWorld(lua_State* L);
    static int open(lua_State* L);
};

} // namespace haylen::lua::physics2d
