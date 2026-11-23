#pragma once

#include <array>
#include <memory>
#include <string_view>

#include "haylen/2d/animation/Chain.hpp"
#include "haylen/lua/Type.hpp"

struct lua_State;

namespace haylen::lua {

template <> struct Type<haylen::animation2d::Chain> {
    static constexpr const char* name = "haylen.Chain";
    using Storage = std::shared_ptr<haylen::animation2d::Chain>;
};

} // namespace haylen::lua

namespace haylen::lua::animation2d {

// Installs the `Chain` class of `haylen.animation2d`.
class ChainBinding final {
  public:
    static void install(lua_State* L);

    // Creates a chain with `animation2d.newChain(options)`.
    static int create(lua_State* L);

  private:
    static constexpr std::array<std::string_view, 7> kOptionFields{"x", "y", "lengths", "segments", "length", "angle", "maxBend"};
    static constexpr std::array<std::string_view, 3> kReachFields{"pole", "iterations", "tolerance"};

    [[nodiscard]] static std::size_t checkPoint(lua_State* L, int index, const haylen::animation2d::Chain& chain);
    static int reach(lua_State* L);
    static int follow(lua_State* L);
    static int straighten(lua_State* L);
    static int point(lua_State* L);
    static int setPoint(lua_State* L);
    static int angle(lua_State* L);
    static int writePoints(lua_State* L);
    static int getX(lua_State* L);
    static int setX(lua_State* L);
    static int getY(lua_State* L);
    static int setY(lua_State* L);
    static int count(lua_State* L);
    static int length(lua_State* L);
};

} // namespace haylen::lua::animation2d
