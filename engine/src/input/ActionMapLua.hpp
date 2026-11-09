#pragma once

#include <array>
#include <memory>
#include <string_view>

#include "haylen/input/ActionMap.hpp"
#include "haylen/lua/Type.hpp"

struct lua_State;

namespace haylen::lua {

template <> struct Type<input::ActionMap> {
    static constexpr const char* name = "haylen.ActionMap";
    using Storage = std::shared_ptr<input::ActionMap>;
};

} // namespace haylen::lua

namespace haylen::input {

// Installs the `ActionMap` class of `haylen.input` and the functions of the module that read and change the default action map, which work like the methods of the map that `input.actions()` returns.
class ActionMapLua final {
  public:
    static void install(lua_State* L);

    // Sets the functions of the action maps on the module table at the top of the stack.
    static void addFunctions(lua_State* L);

  private:
    using Operation = int (*)(lua_State*, ActionMap&, int);

    static constexpr std::array<std::string_view, 5> kDeviceOptions{"keyboard", "mouse", "gamepad", "virtual", "virtualPrefix"};
    static constexpr std::array<std::string_view, 1> kMapOptions{"actions"};

    // Runs an operation on the map of the method call, whose arguments follow the map, or on the default map of the module function.
    template <Operation Run> static int onMap(lua_State* L);
    template <Operation Run> static int onDefault(lua_State* L);

    template <bool (ActionMap::*Read)(std::string_view) const noexcept> static int readFlag(lua_State* L, ActionMap& map, int first);
    static void readDevices(lua_State* L, int index, ActionMap::Devices& devices);
    static void pushDevices(lua_State* L, const ActionMap::Devices& devices);

    static int load(lua_State* L, ActionMap& map, int first);
    static int save(lua_State* L, ActionMap& map, int first);
    static int names(lua_State* L, ActionMap& map, int first);
    static int define(lua_State* L, ActionMap& map, int first);
    static int remove(lua_State* L, ActionMap& map, int first);
    static int clear(lua_State* L, ActionMap& map, int first);
    static int definition(lua_State* L, ActionMap& map, int first);
    static int setPressThreshold(lua_State* L, ActionMap& map, int first);
    static int value(lua_State* L, ActionMap& map, int first);
    static int vector(lua_State* L, ActionMap& map, int first);
    static int consumePress(lua_State* L, ActionMap& map, int first);
    static int stepButtons(lua_State* L, ActionMap& map, int first);

    static int newActionMap(lua_State* L);
    static int actions(lua_State* L);
    static int devices(lua_State* L);
    static int setDevices(lua_State* L);
    static int pressThreshold(lua_State* L);
    static int assignPressThreshold(lua_State* L);
};

} // namespace haylen::input
