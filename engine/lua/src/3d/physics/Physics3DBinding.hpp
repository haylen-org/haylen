#pragma once

#include <lua.hpp>

#include <array>
#include <memory>
#include <optional>
#include <string_view>

#include "3d/physics/ScriptedBody.hpp"
#include "haylen/3d/physics/Body.hpp"
#include "haylen/3d/physics/Joint.hpp"
#include "haylen/3d/physics/Shape.hpp"
#include "haylen/3d/physics/World.hpp"
#include "haylen/core/EnumNames.hpp"
#include "haylen/lua/Type.hpp"

namespace haylen::lua {

template <> struct Type<haylen::physics3d::World> {
    static constexpr const char* name = "haylen.PhysicsWorld3D";
    using Storage = std::shared_ptr<haylen::physics3d::World>;
};

template <> struct Type<lua::physics3d::ScriptedBody> {
    static constexpr const char* name = "haylen.Body3D";
    using Storage = lua::physics3d::ScriptedBody;
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

template <> struct EnumNames<haylen::physics3d::Body::Type> {
    static std::optional<haylen::physics3d::Body::Type> fromName(std::string_view name) {
        return haylen::physics3d::Body::typeFromName(name);
    }
    static std::string_view name(haylen::physics3d::Body::Type value) {
        return haylen::physics3d::Body::typeName(value);
    }
};

template <> struct EnumNames<haylen::physics3d::Body::MotionQuality> {
    static std::optional<haylen::physics3d::Body::MotionQuality> fromName(std::string_view name) {
        return haylen::physics3d::Body::motionQualityFromName(name);
    }
    static std::string_view name(haylen::physics3d::Body::MotionQuality value) {
        return haylen::physics3d::Body::motionQualityName(value);
    }
};

template <> struct EnumNames<haylen::physics3d::Body::CombineRule> {
    static std::optional<haylen::physics3d::Body::CombineRule> fromName(std::string_view name) {
        return haylen::physics3d::Body::combineRuleFromName(name);
    }
    static std::string_view name(haylen::physics3d::Body::CombineRule value) {
        return haylen::physics3d::Body::combineRuleName(value);
    }
};

template <> struct EnumNames<haylen::physics3d::Joint::Type> {
    static std::optional<haylen::physics3d::Joint::Type> fromName(std::string_view name) {
        return haylen::physics3d::Joint::typeFromName(name);
    }
    static std::string_view name(haylen::physics3d::Joint::Type value) {
        return haylen::physics3d::Joint::typeName(value);
    }
};

template <> struct EnumNames<haylen::physics3d::Joint::AxisMode> {
    static std::optional<haylen::physics3d::Joint::AxisMode> fromName(std::string_view name) {
        return haylen::physics3d::Joint::axisModeFromName(name);
    }
    static std::string_view name(haylen::physics3d::Joint::AxisMode value) {
        return haylen::physics3d::Joint::axisModeName(value);
    }
};

template <> struct EnumNames<haylen::physics3d::Joint::Axis> {
    static std::optional<haylen::physics3d::Joint::Axis> fromName(std::string_view name) {
        return haylen::physics3d::Joint::axisFromName(name);
    }
    static std::string_view name(haylen::physics3d::Joint::Axis value) {
        return haylen::physics3d::Joint::axisName(value);
    }
};

template <> struct EnumNames<haylen::physics3d::Joint::SpringMode> {
    static std::optional<haylen::physics3d::Joint::SpringMode> fromName(std::string_view name) {
        return haylen::physics3d::Joint::springModeFromName(name);
    }
    static std::string_view name(haylen::physics3d::Joint::SpringMode value) {
        return haylen::physics3d::Joint::springModeName(value);
    }
};

template <> struct EnumNames<haylen::physics3d::Joint::MotorState> {
    static std::optional<haylen::physics3d::Joint::MotorState> fromName(std::string_view name) {
        return haylen::physics3d::Joint::motorStateFromName(name);
    }
    static std::string_view name(haylen::physics3d::Joint::MotorState value) {
        return haylen::physics3d::Joint::motorStateName(value);
    }
};

template <> struct EnumNames<haylen::physics3d::Joint::PathRotation> {
    static std::optional<haylen::physics3d::Joint::PathRotation> fromName(std::string_view name) {
        return haylen::physics3d::Joint::pathRotationFromName(name);
    }
    static std::string_view name(haylen::physics3d::Joint::PathRotation value) {
        return haylen::physics3d::Joint::pathRotationName(value);
    }
};

} // namespace haylen::core

namespace haylen::lua::physics3d {

// Installs `haylen.physics3d` with the `PhysicsWorld3D` class, the worlds in meters that the selected backend simulates, the `Body3D` class of their bodies, the `Joint3D` class of their joints and the `Shape3D` class of their collision shapes.
class Physics3DBinding final {
  public:
    static void install(lua_State* L);

    // Pushes a body of the Lua world object at `worldIndex`, which the body keeps alive and keeps its data in.
    static void pushBody(lua_State* L, int worldIndex, const haylen::physics3d::Body& body);

  private:
    static constexpr std::array<std::string_view, 1> kOwnerField{"owner"};

    static int newWorld(lua_State* L);
    static int getBackends(lua_State* L);
    static int getCategory(lua_State* L);
    static int getCategories(lua_State* L);
    static int open(lua_State* L);
};

} // namespace haylen::lua::physics3d
