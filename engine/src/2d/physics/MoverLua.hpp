#pragma once

#include <array>
#include <string_view>

struct lua_State;

namespace haylen::physics2d {

class Mover;

// Installs the `Mover` class of `haylen.physics2d`, a kinematic character that slides, climbs steps and rides platforms, which owns the body that follows it.
class MoverLua final {
  public:
    static void install(lua_State* L);

    // Sets `newMover` on the module table at the top of the stack.
    static void addFunctions(lua_State* L);

  private:
    static constexpr std::array<std::string_view, 11> kMoverFields{"x", "y", "radius", "height", "maxSlope", "stepHeight", "snapDistance", "pushForce", "category", "mask", "group"};

    [[nodiscard]] static Mover& check(lua_State* L);

    static int newMover(lua_State* L);
    static int move(lua_State* L);
    static int clip(lua_State* L);
    static int dropThrough(lua_State* L);
    static int destroy(lua_State* L);
    static int getPosition(lua_State* L);
    static int setPosition(lua_State* L);
    static int getX(lua_State* L);
    static int getY(lua_State* L);
    static int isGrounded(lua_State* L);
    static int getGroundNormal(lua_State* L);
    static int getGroundBody(lua_State* L);
    static int getGroundVelocity(lua_State* L);
    static int isOnWall(lua_State* L);
    static int isOnCeiling(lua_State* L);
    static int getBody(lua_State* L);
    static int getRadius(lua_State* L);
    static int getHeight(lua_State* L);
    static int getMaxSlope(lua_State* L);
    static int setMaxSlope(lua_State* L);
    static int getStepHeight(lua_State* L);
    static int getSnapDistance(lua_State* L);
    static int setSnapDistance(lua_State* L);
    static int isValid(lua_State* L);
};

} // namespace haylen::physics2d
