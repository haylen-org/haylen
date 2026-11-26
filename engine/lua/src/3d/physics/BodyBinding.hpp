#pragma once

#include <lua.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

#include "3d/physics/ScriptedBody.hpp"
#include "haylen/3d/physics/Body.hpp"
#include "haylen/lua/Stack.hpp"
#include "haylen/lua/TypeConverter.hpp"

namespace haylen::lua::physics3d {

// Installs the `Body3D` class of `haylen.physics3d` and reads the options that `world:createBody` takes. The data of a body lives in the state table of its world, keyed by the id of the body. Children count from one like a Lua array.
class BodyBinding final {
  public:
    static void install(lua_State* L);

    // Reads the bits of categories at the stack index: an integer of 32 bits, the name of a category of `physics3d.categories` in `haylen.json`, or a list of both, whose bits join.
    [[nodiscard]] static std::uint32_t readCategories(lua_State* L, int index, std::string_view option);

    // Reads the options table at the stack index.
    [[nodiscard]] static haylen::physics3d::Body::Options readOptions(lua_State* L, int index);

    // Returns the live body at the stack index, and raises an error once it or its world was destroyed.
    [[nodiscard]] static haylen::physics3d::Body& check(lua_State* L, int index);

  private:
    static constexpr std::array<std::string_view, 28> kOptionFields{"type", "shape", "position", "rotation", "linearVelocity", "angularVelocity", "motionQuality", "locks", "mass", "inertia", "centerOfMass", "linearDamping", "angularDamping", "gravityScale", "maxLinearSpeed", "maxAngularSpeed", "friction", "restitution", "frictionCombine", "restitutionCombine", "sleepEnabled", "sensor", "collideWithStatic", "enhancedInternalEdgeRemoval", "enabled", "category", "mask", "group"};
    static constexpr std::array<std::string_view, 6> kLockFields{"x", "y", "z", "rotationX", "rotationY", "rotationZ"};

    [[nodiscard]] static std::uint32_t readBits(lua_State* L, int index, std::string_view option);
    [[nodiscard]] static haylen::physics3d::Body::MotionLocks readLocks(lua_State* L, int index);
    static void pushLocks(lua_State* L, const haylen::physics3d::Body::MotionLocks& locks);

    // Pushes the data table of the world of the body at index 1 and the key of the body.
    static void pushDataSlot(lua_State* L, std::uint32_t id);

    // A property that reads a getter of the body.
    template <auto Getter> static int get(lua_State* L) {
        lua::Stack::push(L, (check(L, 1).*Getter)());
        return 1;
    }

    // A property that writes a setter of the body with the value after the key.
    template <auto Setter, typename Value> static int set(lua_State* L) {
        (check(L, 1).*Setter)(lua::Stack::read<Value>(L, 3));
        return 0;
    }

    // A number field of the position, which writes the position with that one number changed.
    template <int Axis> static int getAxis(lua_State* L);
    template <int Axis> static int setAxis(lua_State* L);

    static int isValid(lua_State* L);
    static int getWorld(lua_State* L);
    static int getLocks(lua_State* L);
    static int setLocks(lua_State* L);
    static int getCategory(lua_State* L);
    static int setCategory(lua_State* L);
    static int getMask(lua_State* L);
    static int setMask(lua_State* L);
    static int getGroup(lua_State* L);
    static int setGroup(lua_State* L);
    static int getShape(lua_State* L);
    static int setShape(lua_State* L);
    static int getData(lua_State* L);
    static int setData(lua_State* L);
    static int getChildCount(lua_State* L);
    static int setTransform(lua_State* L);
    static int moveTo(lua_State* L);
    static int applyForce(lua_State* L);
    static int applyImpulse(lua_State* L);
    static int applyTorque(lua_State* L);
    static int applyAngularImpulse(lua_State* L);
    static int velocityAt(lua_State* L);
    static int resetMass(lua_State* L);
    static int addChild(lua_State* L);
    // Returns the child that the one-based index at the stack index names, counted from zero, and raises an error past the last child.
    [[nodiscard]] static std::size_t checkChild(lua_State* L, const haylen::physics3d::Body& body, int index);
    static int removeChild(lua_State* L);
    static int setChildTransform(lua_State* L);
    static int renderTransform(lua_State* L);
    static int destroy(lua_State* L);
    static int equal(lua_State* L);
};

} // namespace haylen::lua::physics3d
