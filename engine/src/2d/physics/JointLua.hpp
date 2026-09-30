#pragma once

struct lua_State;

namespace haylen::physics2d {

// Installs the `Joint` class of `haylen.physics2d`.
class JointLua final {
  public:
    static void install(lua_State* L);

  private:
    static int isValid(lua_State* L);
    static int destroy(lua_State* L);
    static int getTarget(lua_State* L);
    static int setTarget(lua_State* L);
    static int getMotorSpeed(lua_State* L);
    static int setMotorSpeed(lua_State* L);
};

} // namespace haylen::physics2d
