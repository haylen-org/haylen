#include "development/Development.hpp"

#include <utility>

#include "debug/Debugger.hpp"
#include "debug/ExecutionBudget.hpp"

namespace haylen::lua {

// The debugger talks to its client through the connection of the session, and while it halts the app it pumps the connection itself, since no frame pumps it then.
Development::Development(haylen::platform::DevelopmentSession& owner) : executionBudget(std::make_unique<lua::debug::ExecutionBudget>()) {
    // clang-format off
    debugger = std::make_unique<lua::debug::Debugger>(owner, [&owner](const haylen::core::Json& message) {
        owner.sendDebug(message);
    }, [&owner] { owner.pump(); });
    // clang-format on
}

Development::~Development() = default;

Development& Development::of(haylen::platform::DevelopmentSession& session) {
    if (auto* found = dynamic_cast<Development*>(session.getExtension())) {
        return *found;
    }

    auto made = std::make_unique<Development>(session);
    Development& development = *made;
    session.setExtension(std::move(made));
    return development;
}

bool Development::isHalted() const noexcept {
    return debugger->isHalted();
}

void Development::receive(haylen::core::Json message) {
    debugger->receive(std::move(message));
}

void Development::disconnect() {
    debugger->reset();
}

} // namespace haylen::lua
