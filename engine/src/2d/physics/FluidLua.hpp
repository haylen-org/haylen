#pragma once

#include <array>
#include <string_view>

struct lua_State;

namespace haylen::physics2d {

// Installs the `Fluid` class of `haylen.physics2d`, whose particles come out in bulk into float buffers or lists and draw as metaballs without a call per particle.
class FluidLua final {
  public:
    static void install(lua_State* L);

    // Sets `newFluid` on the module table at the top of the stack.
    static void addFunctions(lua_State* L);

  private:
    static constexpr std::array<std::string_view, 15> kFluidFields{"radius", "smoothingRadius", "density", "friction", "restitution", "restDensity", "stiffness", "nearStiffness", "viscosity", "gravityScale", "maxSpeed", "maxParticles", "category", "mask", "group"};
    static constexpr std::array<std::string_view, 5> kDrawFields{"radius", "color", "outlineColor", "outlineWidth", "threshold"};

    // The metaballs of a fluid are this many smoothing radii wide unless its draw call names their radius.
    static constexpr float kBallRadius = 0.6F;

    // Writes two values per particle into the float buffer at index 2 from the position at index 3, or into the list at index 2 or a new one, clearing the entries after them, and leaves it on top.
    static int pushPairs(lua_State* L, bool velocities);

    static int newFluid(lua_State* L);
    static int spawn(lua_State* L);
    static int fill(lua_State* L);
    static int remove(lua_State* L);
    static int clear(lua_State* L);
    static int positions(lua_State* L);
    static int velocities(lua_State* L);
    static int draw(lua_State* L);
    static int size(lua_State* L);
    static int radius(lua_State* L);
    static int stepMilliseconds(lua_State* L);
};

} // namespace haylen::physics2d
