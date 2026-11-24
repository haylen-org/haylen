#pragma once

#include <lua.hpp>

#include <array>
#include <string_view>

#include "haylen/lua/Type.hpp"
#include "haylen/math/Spline2D.hpp"

namespace haylen::lua {

template <> struct Type<haylen::math::Spline2D> {
    static constexpr const char* name = "haylen.Spline2D";
    using Storage = haylen::math::Spline2D;
};

} // namespace haylen::lua

namespace haylen::lua::math {

// Installs the `Spline2D` class of `haylen.math` and its constructor.
class Spline2DBinding final {
  public:
    static void install(lua_State* L);

    // Sets the spline constructor on the module table at the top of the stack.
    static void addFunctions(lua_State* L);

  private:
    static constexpr std::array<std::string_view, 2> kSplineFields{"kind", "closed"};

    static int newSpline(lua_State* L);
    static int point(lua_State* L);
    static int tangent(lua_State* L);
    static int pointAtDistance(lua_State* L);
    static int tangentAtDistance(lua_State* L);
    static int parameterAtDistance(lua_State* L);
    static int sample(lua_State* L);
    static int sampleByDistance(lua_State* L);
    static int flatten(lua_State* L);
    static int getLength(lua_State* L);
    static int getKind(lua_State* L);
    static int isClosed(lua_State* L);
    static int getPoints(lua_State* L);
    static int getSegmentCount(lua_State* L);
};

} // namespace haylen::lua::math
