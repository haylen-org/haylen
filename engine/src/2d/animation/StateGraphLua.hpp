#pragma once

#include <array>
#include <memory>
#include <string_view>

#include "haylen/2d/animation/StateGraph.hpp"
#include "haylen/2d/animation/StateGraphPlayer.hpp"
#include "haylen/lua/Type.hpp"

struct lua_State;

namespace haylen::lua {

template <> struct Type<animation2d::StateGraph> {
    static constexpr const char* name = "haylen.StateGraph";
    using Storage = std::shared_ptr<animation2d::StateGraph>;
};

template <> struct Type<animation2d::StateGraphPlayer> {
    static constexpr const char* name = "haylen.StateGraphPlayer";
    using Storage = std::shared_ptr<animation2d::StateGraphPlayer>;
};

} // namespace haylen::lua

namespace haylen::animation2d {

// Installs the `StateGraph` and `StateGraphPlayer` classes of `haylen.animation2d`.
class StateGraphLua final {
  public:
    static void install(lua_State* L);

    // Creates a graph with `animation2d.newStateGraph(description)`, from a table or the package path of a JSON file.
    static int create(lua_State* L);

  private:
    static constexpr std::array<std::string_view, 1> kPlayerFields{"sprite"};
    static constexpr std::array<std::string_view, 1> kListenerFields{"owner"};

    // The state of the Lua call that moves a player, where its listeners run.
    inline static lua_State* moving = nullptr;

    template <typename Move> static void move(lua_State* L, Move&& body);

    static int states(lua_State* L);
    static int parameters(lua_State* L);
    static int newPlayer(lua_State* L);

    static int set(lua_State* L);
    static int get(lua_State* L);
    static int trigger(lua_State* L);
    static int update(lua_State* L);
    static int enter(lua_State* L);
    static int state(lua_State* L);
    static int stateTime(lua_State* L);
    static int animation(lua_State* L);
    static int on(lua_State* L);
};

} // namespace haylen::animation2d
