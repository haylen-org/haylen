#include "plugins/ExecutionBudgetPlugin.hpp"

#include <limits>

#include "debug/ExecutionBudget.hpp"
#include "development/Development.hpp"
#include "haylen/core/Engine.hpp"
#include "haylen/core/EventBus.hpp"
#include "haylen/core/LifecycleEvent.hpp"
#include "haylen/platform/DevelopmentSession.hpp"

namespace haylen::plugins {

// The app stops its scenes and autoloads before the plugins stop, so the budget gives the code that stops the app its whole time and room in memory once the stop begins, before every listener of the app hears it.
void ExecutionBudgetPlugin::start(core::Engine& engine) {
    if (session == nullptr) {
        return;
    }

    // clang-format off
    stopping = core::ScopedConnection(engine.getEvents().on(core::LifecycleEvent::kAppStopping, [this](core::EventBus::Event&) {
        lua::Development::of(*session).getExecutionBudget().finishApp();
    }, {.priority = std::numeric_limits<int>::max()}));
    // clang-format on
}

void ExecutionBudgetPlugin::stop(core::Engine&) {
    stopping.disconnect();
}

} // namespace haylen::plugins
