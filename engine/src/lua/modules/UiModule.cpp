#include "lua/modules/UiModule.hpp"

#include <limits>

#include "haylen/core/Engine.hpp"
#include "haylen/plugins/UiPlugin.hpp"
#include "ui/ImGuiLua.hpp"
#include "ui/UiLua.hpp"

namespace haylen::lua::modules {

// The handlers run on the main thread of the state, since the coroutine that installed them may be gone by the time an event arrives.
void UiModule::install(core::Engine& engine, lua_State* L) {
    ui::UiLua::install(L);
    ui::ImGuiLua::install(L);

    plugins::UiPlugin& plugin = engine.getPlugin<plugins::UiPlugin>();
    handlers = core::ScopedConnection(plugin.events.connect([L](ui::Gui& gui, const ui::Event& event) { ui::UiLua::deliverEvent(L, gui, event); }, {.priority = std::numeric_limits<int>::min()}));
    unmounted = core::ScopedConnection(plugin.guiUnmounted.connect([L](const ui::Gui& gui) { ui::UiLua::forgetGui(L, gui); }));
}

} // namespace haylen::lua::modules
