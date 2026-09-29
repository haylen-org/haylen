#pragma once

#include <array>
#include <string_view>

struct lua_State;

namespace haylen::physics2d {

// Installs the Fluid class of haylen.physics2d, whose positions and velocities come out in bulk for metaball rendering.
class FluidLua final {
  public:
    static void install(lua_State* L);

    // Sets newFluid on the module table at the top of the stack.
    static void addFunctions(lua_State* L);

  private:
    static constexpr std::array<std::string_view, 13> kFluidFields{"radius", "smoothingRadius", "density", "friction", "restitution", "restDensity", "stiffness", "nearStiffness", "viscosity", "maxParticles", "category", "mask", "group"};

    // Fills the list at index 2, or a new one, with two values per particle and clears what follows them, then leaves it on top.
    static int pushPairs(lua_State* L, bool velocities);

    static int newFluid(lua_State* L);
    static int spawn(lua_State* L);
    static int fill(lua_State* L);
    static int remove(lua_State* L);
    static int clear(lua_State* L);
    static int update(lua_State* L);
    static int positions(lua_State* L);
    static int velocities(lua_State* L);
    static int bodies(lua_State* L);
    static int count(lua_State* L);
    static int radius(lua_State* L);
};

} // namespace haylen::physics2d
