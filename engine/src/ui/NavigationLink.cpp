#include "haylen/ui/NavigationLink.hpp"

#include <utility>

#include "haylen/ui/Message.hpp"
#include "haylen/ui/NavigationSplit.hpp"
#include "haylen/ui/NavigationStack.hpp"
#include "haylen/ui/NavigationTabs.hpp"
#include "haylen/ui/Presentation.hpp"
#include "haylen/ui/Router.hpp"

namespace haylen::ui {

NavigationLink::NavigationLink(Target value) : target(std::move(value)) {}

void NavigationLink::disconnect() {
    // clang-format off
    std::visit([](const auto& weak) {
        const auto held = weak.lock();
        if (!held) {
            return;
        }

        using Held = typename std::decay_t<decltype(held)>::element_type;
        if constexpr (std::is_same_v<Held, Presentation>) {
            held->dismiss(nullptr, NavigationTransition{.effect = NavigationTransition::Effect::None});
        } else if constexpr (std::is_same_v<Held, Message>) {
            held->vanish();
        } else {
            held->close();
        }
    }, target);
    // clang-format on
}

bool NavigationLink::isConnected() const noexcept {
    // clang-format off
    return std::visit([](const auto& weak) {
        const auto held = weak.lock();
        if (!held) {
            return false;
        }

        using Held = typename std::decay_t<decltype(held)>::element_type;
        if constexpr (std::is_same_v<Held, Presentation> || std::is_same_v<Held, Message>) {
            return !held->isDismissed();
        } else {
            return !held->isClosed();
        }
    }, target);
    // clang-format on
}

void NavigationLink::setBlocked(bool) {}

bool NavigationLink::isBlocked() const noexcept {
    return false;
}

} // namespace haylen::ui
