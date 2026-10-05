#pragma once

#include <array>
#include <string_view>

struct lua_State;

namespace haylen::physics2d {

// Installs the `Grabber` class of `haylen.physics2d`, which drags dynamic bodies with a mouse, a finger or a cursor.
class GrabberLua final {
  public:
    static void install(lua_State* L);

    // Sets `newGrabber` on the module table at the top of the stack.
    static void addFunctions(lua_State* L);

  private:
    static constexpr std::array<std::string_view, 6> kGrabberFields{"pickRadius", "strength", "hertz", "dampingRatio", "category", "mask"};

    static int newGrabber(lua_State* L);
    static int grab(lua_State* L);
    static int moveTo(lua_State* L);
    static int release(lua_State* L);
    static int isHolding(lua_State* L);
    static int getBody(lua_State* L);
    static int getTarget(lua_State* L);
    static int getHandle(lua_State* L);
    static int getForce(lua_State* L);
    static int getPickRadius(lua_State* L);
    static int setPickRadius(lua_State* L);
    static int getStrength(lua_State* L);
    static int setStrength(lua_State* L);
    static int getHertz(lua_State* L);
    static int setHertz(lua_State* L);
    static int getDampingRatio(lua_State* L);
    static int setDampingRatio(lua_State* L);
};

} // namespace haylen::physics2d
