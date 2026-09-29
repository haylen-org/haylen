#include "plugins/CorePlugin.hpp"

#include "ai/AiLua.hpp"
#include "core/CollectionsLua.hpp"
#include "core/CoreLua.hpp"
#include "core/EventsLua.hpp"
#include "core/SceneLua.hpp"
#include "core/SignalLua.hpp"
#include "core/TimerLua.hpp"
#include "core/TweenLua.hpp"
#include "math/MathLua.hpp"

namespace haylen::plugins {

void CorePlugin::stop(core::Engine& engine) {
    autoloads.stop(engine);
}

void CorePlugin::installLua(core::Engine&, lua_State* L) {
    core::CoreLua::install(L);
    core::CollectionsLua::install(L);
    core::TimerLua::install(L);
    core::SceneLua::install(L);
    core::SignalLua::install(L);
    core::EventsLua::install(L);
    math::MathLua::install(L);
    core::TweenLua::install(L);
    ai::AiLua::install(L);
}

void CorePlugin::event(core::Engine& engine, const platform::Event& event) {
    autoloads.event(engine, event);
}

void CorePlugin::fixedUpdate(core::Engine& engine, float stepSeconds) {
    autoloads.fixedUpdate(engine, stepSeconds);
}

void CorePlugin::update(core::Engine& engine, float deltaSeconds) {
    autoloads.update(engine, deltaSeconds);
}

void CorePlugin::render(core::Engine&) {
    autoloads.render();
}

void CorePlugin::renderUi(core::Engine&) {
    autoloads.renderUi();
}

} // namespace haylen::plugins
