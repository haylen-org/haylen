#include "ai/AiLua.hpp"

#include <string>
#include <utility>

#include "ai/BehaviorTreeLua.hpp"
#include "ai/InfluenceMapLua.hpp"
#include "ai/UtilityLua.hpp"
#include "haylen/lua/Binding.hpp"
#include "haylen/lua/ClassBuilder.hpp"
#include "haylen/lua/Stack.hpp"
#include "haylen/lua/Type.hpp"
#include "haylen/lua/Userdata.hpp"

namespace haylen::lua {

template <> struct Type<ai::AiLua::Scripted> {
    static constexpr const char* name = "haylen.StateMachine";
    using Storage = ai::AiLua::Scripted;
};

} // namespace haylen::lua

namespace haylen::ai {

// Keeps the calling thread for the callbacks, restoring the outer one when methods nest.
class AiLua::CallerScope final {
  public:
    CallerScope(Scripted& machine, lua_State* L) : owner(machine), outer(std::exchange(machine.caller, L)) {}
    ~CallerScope() {
        owner.caller = outer;
    }

    CallerScope(const CallerScope&) = delete;
    CallerScope& operator=(const CallerScope&) = delete;

  private:
    Scripted& owner;
    lua_State* outer;
};

bool AiLua::pushStateFunction(lua_State* L, const std::string& name, const char* callback) {
    lua::Userdata::pushField(L, 1, "states");
    lua_getfield(L, -1, name.c_str());
    if (!lua_istable(L, -1)) {
        lua_pop(L, 2);
        return false;
    }
    lua_getfield(L, -1, callback);
    lua_remove(L, -2);
    lua_remove(L, -2);
    if (lua_isfunction(L, -1)) {
        return true;
    }
    lua_pop(L, 1);
    return false;
}

// Each change leaves its extra arguments at the back of a queue, and each entered state takes the front, so queued changes keep their own arguments.
void AiLua::enterState(Scripted& self, const std::string& name) {
    lua_State* L = self.caller;
    lua::Userdata::pushField(L, 1, "arguments");
    const int queue = lua_gettop(L);
    lua_rawgeti(L, queue, 1);
    const int arguments = lua_gettop(L);
    const auto remaining = luaL_len(L, queue);
    for (lua_Integer index = 1; index < remaining; ++index) {
        lua_rawgeti(L, queue, index + 1);
        lua_rawseti(L, queue, index);
    }
    lua_pushnil(L);
    lua_rawseti(L, queue, remaining);

    if (!pushStateFunction(L, name, "enter")) {
        lua_settop(L, queue - 1);
        return;
    }
    lua_pushvalue(L, 1);
    lua_getfield(L, arguments, "n");
    const auto count = static_cast<int>(lua_tointeger(L, -1));
    lua_pop(L, 1);
    luaL_checkstack(L, count, "too many state arguments");
    for (int index = 1; index <= count; ++index) {
        lua_rawgeti(L, arguments, index);
    }
    lua_call(L, count + 1, 0);
    lua_settop(L, queue - 1);
}

void AiLua::callState(Scripted& self, const std::string& name, const char* callback, const float* deltaSeconds) {
    lua_State* L = self.caller;
    if (!pushStateFunction(L, name, callback)) {
        return;
    }
    lua_pushvalue(L, 1);
    if (deltaSeconds != nullptr) {
        lua::Stack::push(L, *deltaSeconds);
    }
    lua_call(L, deltaSeconds != nullptr ? 2 : 1, 0);
}

// Creates a machine with `newStateMachine(states)`, where `states` maps names to tables with optional `enter(machine, ...)`, `update(machine, dt)` and `exit(machine)` functions.
int AiLua::newStateMachine(lua_State* L) {
    luaL_checktype(L, 1, LUA_TTABLE);
    Scripted& self = lua::Userdata::emplace<Scripted>(L);
    const int owner = lua_gettop(L);
    lua::Userdata::setField(L, owner, "states", 1);
    lua_newtable(L);
    lua::Userdata::setField(L, owner, "arguments", -1);
    lua_pop(L, 1);

    lua_pushnil(L);
    while (lua_next(L, 1) != 0) {
        luaL_argcheck(L, lua_type(L, -2) == LUA_TSTRING, 1, "state names must be strings");
        luaL_argcheck(L, lua_istable(L, -1), 1, "each state must be a table");
        std::string name = lua::Stack::read<std::string>(L, -2);
        // clang-format off
        self.machine.add(name, {
            .enter = [&self, name] { enterState(self, name); },
            .update = [&self, name](float deltaSeconds) { callState(self, name, "update", &deltaSeconds); },
            .exit = [&self, name] { callState(self, name, "exit", nullptr); },
        });
        // clang-format on
        lua_pop(L, 1);
    }

    // clang-format off
    self.machine.onChange = [&self](std::string_view from, std::string_view to) {
        lua_State* caller = self.caller;
        if (!lua::Userdata::pushFunction(caller, 1, "onChange")) {
            return;
        }
        lua_pushvalue(caller, 1);
        if (from.empty()) {
            lua_pushnil(caller);
        } else {
            lua::Stack::push(caller, from);
        }
        lua::Stack::push(caller, to);
        lua_call(caller, 3, 0);
    };
    // clang-format on
    return 1;
}

// Changes state with `change(name, ...)`, passing the extra arguments to the `enter` function of the new state.
int AiLua::machineChange(lua_State* L) {
    Scripted& self = lua::Userdata::check<Scripted>(L, 1);
    const std::string name = lua::Stack::read<std::string>(L, 2);
    if (!self.machine.has(name)) {
        return luaL_error(L, "The state machine has no state named \"%s\".", name.c_str());
    }

    const int count = lua_gettop(L) - 2;
    lua_createtable(L, count, 1);
    for (int index = 1; index <= count; ++index) {
        lua_pushvalue(L, 2 + index);
        lua_rawseti(L, -2, index);
    }
    lua_pushinteger(L, count);
    lua_setfield(L, -2, "n");
    const int arguments = lua_gettop(L);

    // A change that starts while no other one runs begins with an empty queue, dropping arguments that a failed transition left behind.
    if (self.changesInProgress == 0) {
        lua_newtable(L);
        lua::Userdata::setField(L, 1, "arguments", -1);
    } else {
        lua::Userdata::pushField(L, 1, "arguments");
    }
    lua_pushvalue(L, arguments);
    lua_rawseti(L, -2, luaL_len(L, -2) + 1);
    lua_settop(L, 1);

    struct ChangeScope {
        int& counter;
        ~ChangeScope() {
            --counter;
        }
    } changing{++self.changesInProgress};
    const CallerScope caller(self, L);
    self.machine.change(name);
    return 0;
}

int AiLua::machineUpdate(lua_State* L) {
    Scripted& self = lua::Userdata::check<Scripted>(L, 1);
    const auto deltaSeconds = lua::Stack::read<float>(L, 2);
    lua_settop(L, 1);
    const CallerScope caller(self, L);
    self.machine.update(deltaSeconds);
    return 0;
}

int AiLua::machineHas(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<Scripted>(L, 1).machine.has(lua::Stack::read<std::string_view>(L, 2)));
    return 1;
}

void AiLua::pushName(lua_State* L, const std::string& name) {
    if (name.empty()) {
        lua_pushnil(L);
        return;
    }
    lua::Stack::push(L, name);
}

int AiLua::machineCurrent(lua_State* L) {
    pushName(L, lua::Userdata::check<Scripted>(L, 1).machine.getCurrent());
    return 1;
}

int AiLua::machinePrevious(lua_State* L) {
    pushName(L, lua::Userdata::check<Scripted>(L, 1).machine.getPrevious());
    return 1;
}

int AiLua::machineElapsed(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<Scripted>(L, 1).machine.getElapsed());
    return 1;
}

int AiLua::machineGetOnChange(lua_State* L) {
    (void)lua::Userdata::check<Scripted>(L, 1);
    lua::Userdata::pushField(L, 1, "onChange");
    return 1;
}

int AiLua::machineSetOnChange(lua_State* L) {
    (void)lua::Userdata::check<Scripted>(L, 1);
    if (!lua_isnil(L, 3)) {
        luaL_checktype(L, 3, LUA_TFUNCTION);
    }
    lua::Userdata::setField(L, 1, "onChange", 3);
    return 0;
}

int AiLua::open(lua_State* L) {
    const luaL_Reg functions[] = {
        {"newStateMachine", &lua::Binding::native<&newStateMachine>},
        {nullptr, nullptr},
    };
    lua::Binding::newModule(L, functions);
    BehaviorTreeLua::addFunctions(L);
    UtilityLua::addFunctions(L);
    InfluenceMapLua::addFunctions(L);
    return 1;
}

void AiLua::install(lua_State* L) {
    lua::ClassBuilder<Scripted>(L).function("change", &lua::Binding::native<&machineChange>).function("update", &lua::Binding::native<&machineUpdate>).function("has", &lua::Binding::native<&machineHas>).property("current", &machineCurrent).property("previous", &machinePrevious).property("elapsed", &machineElapsed).property("onChange", &machineGetOnChange, &machineSetOnChange).install();
    BehaviorTreeLua::install(L);
    UtilityLua::install(L);
    InfluenceMapLua::install(L);
    lua::Binding::preload(L, "haylen.ai", &open);
}

} // namespace haylen::ai
