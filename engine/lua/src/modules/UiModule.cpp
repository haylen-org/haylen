#include "modules/UiModule.hpp"

#include <limits>

#include "haylen/core/Engine.hpp"
#include "haylen/plugins/UiPlugin.hpp"
#include "ui/ImGuiBinding.hpp"
#include "ui/UiBinding.hpp"

namespace haylen::lua::modules {

// The handlers run on the main thread of the state, since the coroutine that installed them may be gone by the time an event arrives.
void UiModule::install(haylen::core::Engine& engine, lua_State* L) {
    lua::ui::UiBinding::install(L);
    lua::ui::ImGuiBinding::install(L);

    haylen::plugins::UiPlugin& plugin = engine.getPlugin<haylen::plugins::UiPlugin>();
    handlers = haylen::core::ScopedConnection(plugin.events.connect([L](haylen::ui::Gui& gui, const haylen::ui::Event& event) { lua::ui::UiBinding::deliverEvent(L, gui, event); }, {.priority = std::numeric_limits<int>::min()}));
    unmounted = haylen::core::ScopedConnection(plugin.guiUnmounted.connect([L](const haylen::ui::Gui& gui) { lua::ui::UiBinding::forgetGui(L, gui); }));
}

} // namespace haylen::lua::modules
