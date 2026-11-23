#pragma once

#include "haylen/2d/navigation/SteeringAgent.hpp"
#include "haylen/2d/navigation/Wanderer.hpp"

namespace haylen::lua::navigation2d {

// A steering agent created from Lua. It carries its own wandering state, so `agent:wander(dt)` needs nothing else.
struct ScriptedAgent {
    haylen::navigation2d::SteeringAgent agent;
    haylen::navigation2d::Wanderer wanderer;
};

} // namespace haylen::lua::navigation2d
