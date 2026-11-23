#include "lua/modules/CoreModule.hpp"

#include "ai/AiLua.hpp"
#include "core/CollectionsLua.hpp"
#include "core/CoreLua.hpp"
#include "core/EventsLua.hpp"
#include "core/SceneLua.hpp"
#include "core/SignalLua.hpp"
#include "core/TimerLua.hpp"
#include "core/TweenLua.hpp"
#include "math/MathLua.hpp"
#include "platform/DisplayLua.hpp"
#include "platform/WindowLua.hpp"

namespace haylen::lua::modules {

void CoreModule::install(core::Engine&, lua_State* L) {
    core::CoreLua::install(L);
    platform::WindowLua::install(L);
    platform::DisplayLua::install(L);
    core::CollectionsLua::install(L);
    core::TimerLua::install(L);
    core::SceneLua::install(L);
    core::SignalLua::install(L);
    core::EventsLua::install(L);
    math::MathLua::install(L);
    core::TweenLua::install(L);
    ai::AiLua::install(L);
}

} // namespace haylen::lua::modules
