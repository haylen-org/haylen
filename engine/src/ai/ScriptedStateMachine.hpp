#pragma once

#include "haylen/ai/StateMachine.hpp"

struct lua_State;

namespace haylen::ai {

// A state machine created from Lua. Its callbacks run on whichever Lua thread is inside a machine method, with the machine at stack index 1.
struct ScriptedStateMachine {
    StateMachine machine;
    lua_State* caller = nullptr;
    int changesInProgress = 0;
};

} // namespace haylen::ai
