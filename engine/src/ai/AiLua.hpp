#pragma once

#include <string>

#include "haylen/ai/StateMachine.hpp"

struct lua_State;

namespace haylen::ai {

// Installs haylen.ai with the StateMachine class, whose states are Lua tables of enter, update and exit functions, and the behavior trees, utility selectors and influence maps of the other AI bindings.
class AiLua final {
  public:
    // A state machine created from Lua. Its callbacks run on whichever Lua thread is inside a machine method, with the machine at stack index 1.
    struct Scripted {
        StateMachine machine;
        lua_State* caller = nullptr;
        int changesInProgress = 0;
    };

    static void install(lua_State* L);

  private:
    class CallerScope;

    // Pushes states[name][callback] and returns true, or leaves the stack unchanged when the state has no such function.
    [[nodiscard]] static bool pushStateFunction(lua_State* L, const std::string& name, const char* callback);
    static void enterState(Scripted& self, const std::string& name);
    static void callState(Scripted& self, const std::string& name, const char* callback, const float* deltaSeconds);
    static void pushName(lua_State* L, const std::string& name);

    static int newStateMachine(lua_State* L);
    static int machineChange(lua_State* L);
    static int machineUpdate(lua_State* L);
    static int machineHas(lua_State* L);
    static int machineState(lua_State* L);
    static int machinePrevious(lua_State* L);
    static int machineElapsed(lua_State* L);
    static int machineGetOnChange(lua_State* L);
    static int machineSetOnChange(lua_State* L);
    static int open(lua_State* L);
};

} // namespace haylen::ai
