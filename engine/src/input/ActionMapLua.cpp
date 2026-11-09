#include "input/ActionMapLua.hpp"

#include <lua.hpp>

#include <cstdint>
#include <string>

#include "haylen/assets/Manager.hpp"
#include "haylen/core/Engine.hpp"
#include "haylen/input/ActionMaps.hpp"
#include "haylen/input/Input.hpp"
#include "haylen/lua/Binding.hpp"
#include "haylen/lua/ClassBuilder.hpp"
#include "haylen/lua/JsonConverter.hpp"
#include "haylen/lua/Runtime.hpp"
#include "haylen/lua/Stack.hpp"
#include "haylen/lua/Table.hpp"
#include "haylen/lua/Userdata.hpp"

namespace haylen::input {

template <ActionMapLua::Operation Run> int ActionMapLua::onMap(lua_State* L) {
    return lua::Binding::guarded(L, [L] { return Run(L, lua::Userdata::check<ActionMap>(L, 1), 2); });
}

template <ActionMapLua::Operation Run> int ActionMapLua::onDefault(lua_State* L) {
    return lua::Binding::guarded(L, [L] { return Run(L, lua::Runtime::getEngine(L).getActions(), 1); });
}

template <bool (ActionMap::*Read)(std::string_view) const noexcept> int ActionMapLua::readFlag(lua_State* L, ActionMap& map, int first) {
    lua::Stack::push(L, (map.*Read)(lua::Stack::read<std::string_view>(L, first)));
    return 1;
}

// Reads `{keyboard, mouse, gamepad, virtual, virtualPrefix}`, where `gamepad` is an index from 1, `false` for none, or left out for every gamepad.
void ActionMapLua::readDevices(lua_State* L, int index, ActionMap::Devices& devices) {
    lua::Table::readField(L, index, "keyboard", devices.keyboard);
    lua::Table::readField(L, index, "mouse", devices.mouse);
    lua::Table::readField(L, index, "virtual", devices.virtualControls);
    lua::Table::readField(L, index, "virtualPrefix", devices.virtualPrefix);

    const int type = lua_getfield(L, index, "gamepad");
    if (type == LUA_TBOOLEAN && lua_toboolean(L, -1) == 0) {
        devices.gamepads = ActionMap::Devices::Gamepads::None;
    } else if (type != LUA_TNIL) {
        int isInteger = 0;
        const lua_Integer gamepad = lua_tointegerx(L, -1, &isInteger);
        if (isInteger == 0 || gamepad < 1 || gamepad > static_cast<lua_Integer>(Input::kMaxGamepads)) {
            luaL_error(L, "The \"gamepad\" of an action map must be an index from 1 to %d, or false for none.", static_cast<int>(Input::kMaxGamepads));
        }
        devices.gamepads = ActionMap::Devices::Gamepads::One;
        devices.gamepad = static_cast<std::size_t>(gamepad - 1);
    }
    lua_pop(L, 1);
}

void ActionMapLua::pushDevices(lua_State* L, const ActionMap::Devices& devices) {
    lua_createtable(L, 0, 5);
    lua::Stack::push(L, devices.keyboard);
    lua_setfield(L, -2, "keyboard");
    lua::Stack::push(L, devices.mouse);
    lua_setfield(L, -2, "mouse");
    lua::Stack::push(L, devices.virtualControls);
    lua_setfield(L, -2, "virtual");
    lua::Stack::push(L, devices.virtualPrefix);
    lua_setfield(L, -2, "virtualPrefix");
    if (devices.gamepads == ActionMap::Devices::Gamepads::One) {
        lua_pushinteger(L, static_cast<lua_Integer>(devices.gamepad + 1));
        lua_setfield(L, -2, "gamepad");
    } else if (devices.gamepads == ActionMap::Devices::Gamepads::None) {
        lua_pushboolean(L, 0);
        lua_setfield(L, -2, "gamepad");
    }
}

// Takes a document table or the path of a JSON file in the content folder.
int ActionMapLua::load(lua_State* L, ActionMap& map, int first) {
    map.load(lua_type(L, first) == LUA_TSTRING ? lua::Runtime::getEngine(L).getAssets().json(lua::Stack::read<std::string_view>(L, first)) : lua::JsonConverter::read(L, first));
    return 0;
}

int ActionMapLua::save(lua_State* L, ActionMap& map, int) {
    lua::JsonConverter::push(L, map.save());
    return 1;
}

int ActionMapLua::names(lua_State* L, ActionMap& map, int) {
    lua::Stack::push(L, map.getNames());
    return 1;
}

// Adds an action given as a table in the action map document format, or replaces the action with the same name in its place.
int ActionMapLua::define(lua_State* L, ActionMap& map, int first) {
    luaL_checktype(L, first, LUA_TTABLE);
    map.define(ActionMap::Action::fromJson(lua::JsonConverter::read(L, first)));
    return 0;
}

int ActionMapLua::remove(lua_State* L, ActionMap& map, int first) {
    map.remove(lua::Stack::read<std::string_view>(L, first));
    return 0;
}

int ActionMapLua::clear(lua_State*, ActionMap& map, int) {
    map.clear();
    return 0;
}

int ActionMapLua::definition(lua_State* L, ActionMap& map, int first) {
    const ActionMap::Action* found = map.findAction(lua::Stack::read<std::string_view>(L, first));
    if (found == nullptr) {
        lua_pushnil(L);
        return 1;
    }
    lua::JsonConverter::push(L, found->toJson());
    return 1;
}

int ActionMapLua::setPressThreshold(lua_State* L, ActionMap& map, int first) {
    map.setPressThreshold(lua::Stack::read<float>(L, first));
    return 0;
}

int ActionMapLua::value(lua_State* L, ActionMap& map, int first) {
    lua::Stack::push(L, map.getValue(lua::Stack::read<std::string_view>(L, first)));
    return 1;
}

int ActionMapLua::vector(lua_State* L, ActionMap& map, int first) {
    const math::Vec2 direction = map.getVector(lua::Stack::read<std::string_view>(L, first));
    lua::Stack::push(L, direction.x);
    lua::Stack::push(L, direction.y);
    return 2;
}

int ActionMapLua::consumePress(lua_State* L, ActionMap& map, int first) {
    lua::Stack::push(L, map.consumePress(lua::Stack::read<std::string_view>(L, first)));
    return 1;
}

// Returns one integer whose bit `n - 1` is set when the action `list[n]` is down in this fixed step, for recorders, command buffers and bots that keep one number per player and step.
int ActionMapLua::stepButtons(lua_State* L, ActionMap& map, int first) {
    luaL_checktype(L, first, LUA_TTABLE);
    const auto count = static_cast<lua_Integer>(lua_rawlen(L, first));
    luaL_argcheck(L, count <= 32, first, "at most 32 actions fit in the buttons of a step");
    std::uint32_t mask = 0;
    for (lua_Integer index = 1; index <= count; ++index) {
        std::size_t length = 0;
        const char* name = lua_rawgeti(L, first, index) == LUA_TSTRING ? lua_tolstring(L, -1, &length) : nullptr;
        if (name == nullptr) {
            return luaL_error(L, "The list of the buttons of a step holds action names, and entry %d is not one.", static_cast<int>(index));
        }
        if (map.isStepDown({name, length})) {
            mask |= 1U << static_cast<unsigned>(index - 1);
        }
        lua_pop(L, 1);
    }
    lua_pushinteger(L, static_cast<lua_Integer>(mask));
    return 1;
}

// Creates a map with `newActionMap({keyboard, mouse, gamepad, virtual, virtualPrefix, actions})`, which reads every device by default and loads the document or file of `actions` when it is given.
int ActionMapLua::newActionMap(lua_State* L) {
    ActionMap::Devices devices;
    if (!lua_isnoneornil(L, 1)) {
        luaL_checktype(L, 1, LUA_TTABLE);
        lua::Table::checkFields(L, 1, {kDeviceOptions, kMapOptions});
        readDevices(L, 1, devices);
    }
    core::Engine& engine = lua::Runtime::getEngine(L);
    std::shared_ptr<ActionMap> map = engine.getActionMaps().create(devices);
    if (!lua_isnoneornil(L, 1) && lua_getfield(L, 1, "actions") != LUA_TNIL) {
        load(L, *map, lua_gettop(L));
    }
    lua::Userdata::emplace<ActionMap>(L, std::move(map));
    return 1;
}

int ActionMapLua::actions(lua_State* L) {
    lua::Userdata::emplace<ActionMap>(L, lua::Runtime::getEngine(L).getActionMaps().getDefault());
    return 1;
}

int ActionMapLua::devices(lua_State* L) {
    pushDevices(L, lua::Userdata::check<ActionMap>(L, 1).getDevices());
    return 1;
}

int ActionMapLua::setDevices(lua_State* L) {
    ActionMap& map = lua::Userdata::check<ActionMap>(L, 1);
    luaL_checktype(L, 3, LUA_TTABLE);
    lua::Table::checkFields(L, 3, {kDeviceOptions});
    ActionMap::Devices devices;
    readDevices(L, 3, devices);
    map.setDevices(std::move(devices));
    return 0;
}

int ActionMapLua::pressThreshold(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<ActionMap>(L, 1).getPressThreshold());
    return 1;
}

int ActionMapLua::assignPressThreshold(lua_State* L) {
    lua::Userdata::check<ActionMap>(L, 1).setPressThreshold(lua::Stack::read<float>(L, 3));
    return 0;
}

void ActionMapLua::install(lua_State* L) {
    lua::ClassBuilder<ActionMap>(L).function("load", &onMap<&load>).function("save", &onMap<&save>).function("names", &onMap<&names>).function("define", &onMap<&define>).function("remove", &onMap<&remove>).function("clear", &onMap<&clear>).function("definition", &onMap<&definition>).function("down", &onMap<&readFlag<&ActionMap::isDown>>).function("pressed", &onMap<&readFlag<&ActionMap::isPressed>>).function("released", &onMap<&readFlag<&ActionMap::isReleased>>).function("value", &onMap<&value>).function("vector", &onMap<&vector>).function("stepDown", &onMap<&readFlag<&ActionMap::isStepDown>>).function("stepPressed", &onMap<&readFlag<&ActionMap::isStepPressed>>).function("stepReleased", &onMap<&readFlag<&ActionMap::isStepReleased>>).function("consumePress", &onMap<&consumePress>).function("stepButtons", &onMap<&stepButtons>).property("devices", &devices, &lua::Binding::native<&setDevices>).property("pressThreshold", &pressThreshold, &lua::Binding::native<&assignPressThreshold>).meta("__eq", &lua::Userdata::equal<ActionMap>).install();
}

void ActionMapLua::addFunctions(lua_State* L) {
    const luaL_Reg functions[] = {
        {"newActionMap", &lua::Binding::native<&newActionMap>}, {"actions", &actions}, {"loadActions", &onDefault<&load>}, {"saveActions", &onDefault<&save>}, {"actionNames", &onDefault<&names>}, {"defineAction", &onDefault<&define>}, {"removeAction", &onDefault<&remove>}, {"clearActions", &onDefault<&clear>}, {"actionDefinition", &onDefault<&definition>}, {"setPressThreshold", &onDefault<&setPressThreshold>}, {"down", &onDefault<&readFlag<&ActionMap::isDown>>}, {"pressed", &onDefault<&readFlag<&ActionMap::isPressed>>}, {"released", &onDefault<&readFlag<&ActionMap::isReleased>>}, {"value", &onDefault<&value>}, {"vector", &onDefault<&vector>}, {"stepDown", &onDefault<&readFlag<&ActionMap::isStepDown>>}, {"stepPressed", &onDefault<&readFlag<&ActionMap::isStepPressed>>}, {"stepReleased", &onDefault<&readFlag<&ActionMap::isStepReleased>>}, {"consumePress", &onDefault<&consumePress>}, {nullptr, nullptr},
    };
    luaL_setfuncs(L, functions, 0);
}

} // namespace haylen::input
