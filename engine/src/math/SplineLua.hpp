#pragma once

#include <lua.hpp>

#include <array>
#include <string_view>

namespace haylen::math {

class Spline;

// Installs the Spline class of haylen.math and its constructor.
class SplineLua final {
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
    static int getLength(lua_State* L);
    static int getKind(lua_State* L);
    static int isClosed(lua_State* L);
    static int getPoints(lua_State* L);
    static int getSegmentCount(lua_State* L);
};

} // namespace haylen::math
