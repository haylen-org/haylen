#pragma once

#include "haylen/2d/navigation/SteeringAgent.hpp"
#include "haylen/2d/navigation/Wanderer.hpp"

namespace haylen::navigation2d {

// A steering agent created from Lua. It carries its own wandering state, so agent:wander(dt) needs nothing else.
struct ScriptedAgent {
    SteeringAgent agent;
    Wanderer wanderer;
};

} // namespace haylen::navigation2d
