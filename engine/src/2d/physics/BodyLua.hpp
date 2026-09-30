#pragma once

#include <cstdint>
#include <optional>
#include <vector>

#include "haylen/2d/physics/Body.hpp"
#include "haylen/2d/physics/Shape.hpp"
#include "haylen/math/Vec2.hpp"

struct lua_State;

namespace haylen::physics2d {

// Installs the `Body` class of `haylen.physics2d`. The data of a body lives in the state table of its world, keyed by the body id.
class BodyLua final {
  public:
    static void install(lua_State* L);

  private:
    [[nodiscard]] static Body& check(lua_State* L);

    // Pushes the data table of the world that owns the body at index 1 and the key of the body.
    static void pushDataSlot(lua_State* L, std::uint64_t id);
    [[nodiscard]] static std::optional<math::Vec2> readOptionalPoint(lua_State* L, int index);
    static int pushNewShape(lua_State* L, Shape shape);
    static int pushNewShapes(lua_State* L, const std::vector<Shape>& created);

    static int isValid(lua_State* L);
    static int getWorld(lua_State* L);
    static int getType(lua_State* L);
    static int setType(lua_State* L);
    static int getX(lua_State* L);
    static int setX(lua_State* L);
    static int getY(lua_State* L);
    static int setY(lua_State* L);
    static int getPosition(lua_State* L);
    static int setPosition(lua_State* L);
    static int getRotation(lua_State* L);
    static int setRotation(lua_State* L);
    static int getVelocity(lua_State* L);
    static int setVelocity(lua_State* L);
    static int getAngularVelocity(lua_State* L);
    static int setAngularVelocity(lua_State* L);
    static int getMass(lua_State* L);
    static int isAwake(lua_State* L);
    static int setAwake(lua_State* L);
    static int isEnabled(lua_State* L);
    static int setEnabled(lua_State* L);
    static int getLinearDamping(lua_State* L);
    static int setLinearDamping(lua_State* L);
    static int getAngularDamping(lua_State* L);
    static int setAngularDamping(lua_State* L);
    static int getGravityScale(lua_State* L);
    static int setGravityScale(lua_State* L);
    static int isFixedRotation(lua_State* L);
    static int setFixedRotation(lua_State* L);
    static int isBullet(lua_State* L);
    static int setBullet(lua_State* L);
    static int getData(lua_State* L);
    static int setData(lua_State* L);

    static int applyForce(lua_State* L);
    static int applyImpulse(lua_State* L);
    static int applyTorque(lua_State* L);
    static int applyAngularImpulse(lua_State* L);
    static int setTransform(lua_State* L);
    static int addBox(lua_State* L);
    static int addCircle(lua_State* L);
    static int addCapsule(lua_State* L);
    static int addSegment(lua_State* L);
    static int addPolygon(lua_State* L);
    static int addChain(lua_State* L);
    static int shapes(lua_State* L);
    static int outlines(lua_State* L);
    static int destroy(lua_State* L);
    static int equal(lua_State* L);
};

} // namespace haylen::physics2d
