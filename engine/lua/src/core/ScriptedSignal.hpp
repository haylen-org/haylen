#pragma once

#include <lua.hpp>

#include <string>

#include "haylen/core/Signal.hpp"

namespace haylen::core {

// A signal created from Lua. Its slots receive the emitted values where they sit on the stack of the emitting thread, so an emit copies nothing.
struct ScriptedSignal {
    struct Arguments {
        lua_State* state = nullptr;
        int first = 0;
        int count = 0;
    };

    Signal<const Arguments&> signal;
    std::string name;
};

} // namespace haylen::core
