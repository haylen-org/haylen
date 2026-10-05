#pragma once

#include <array>
#include <string_view>

struct lua_State;

namespace haylen::physics2d {

class ForceField;

// Installs the `ForceField` class of `haylen.physics2d`: areas that pull, push, swirl and float the bodies inside them every step.
class ForceFieldLua final {
  public:
    static void install(lua_State* L);

    // Sets `newForceField` on the module table at the top of the stack.
    static void addFunctions(lua_State* L);

  private:
    static constexpr std::array<std::string_view, 20> kFieldFields{"kind", "x", "y", "radius", "width", "height", "points", "strength", "direction", "falloff", "minDistance", "acceleration", "density", "linearDrag", "angularDrag", "flow", "category", "mask", "group", "enabled"};

    [[nodiscard]] static ForceField& check(lua_State* L);

    static int newForceField(lua_State* L);
    static int destroy(lua_State* L);
    static int isValid(lua_State* L);
    static int getKind(lua_State* L);
    static int isEnabled(lua_State* L);
    static int setEnabled(lua_State* L);
    static int getStrength(lua_State* L);
    static int setStrength(lua_State* L);
    static int getPosition(lua_State* L);
    static int setPosition(lua_State* L);
    static int getDirection(lua_State* L);
    static int setDirection(lua_State* L);
    static int getFlow(lua_State* L);
    static int setFlow(lua_State* L);
    static int getDensity(lua_State* L);
    static int setDensity(lua_State* L);
    static int getLinearDrag(lua_State* L);
    static int setLinearDrag(lua_State* L);
    static int getAngularDrag(lua_State* L);
    static int setAngularDrag(lua_State* L);
    static int getBounds(lua_State* L);
    static int getBodyCount(lua_State* L);
};

} // namespace haylen::physics2d
