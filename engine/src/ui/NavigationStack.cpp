#include "haylen/ui/NavigationStack.hpp"

#include <algorithm>
#include <iterator>
#include <stdexcept>
#include <utility>

#include "haylen/ui/Navigation.hpp"
#include "haylen/ui/Screen.hpp"

namespace haylen::ui {

NavigationStack::NavigationStack(Navigation& owner, Region value) : navigation(owner), region(std::move(value)) {}

NavigationStack::~NavigationStack() = default;

void NavigationStack::push(std::shared_ptr<Screen> screen, const NavigationTransition& transition) {
    checkOpen();
    std::vector<std::shared_ptr<Screen>> next = screens;
    next.push_back(std::move(screen));
    apply(std::move(next), transition);
}

// A stack with one screen or none stays as it is, since nothing lies under its top screen.
void NavigationStack::pop(const NavigationTransition& transition) {
    checkOpen();
    if (screens.size() < 2) {
        return;
    }

    apply({screens.begin(), std::prev(screens.end())}, transition);
}

void NavigationStack::popTo(const Screen& screen, const NavigationTransition& transition) {
    checkOpen();
    const auto found = std::ranges::find(screens, &screen, [](const std::shared_ptr<Screen>& entry) { return entry.get(); });
    if (found == screens.end()) {
        throw std::invalid_argument("The navigation stack does not hold the screen to pop to.");
    }

    apply({screens.begin(), std::next(found)}, transition);
}

void NavigationStack::popToRoot(const NavigationTransition& transition) {
    checkOpen();
    if (screens.size() < 2) {
        return;
    }

    apply({screens.front()}, transition);
}

void NavigationStack::replace(std::shared_ptr<Screen> screen, const NavigationTransition& transition) {
    checkOpen();
    std::vector<std::shared_ptr<Screen>> next = screens;
    if (!next.empty()) {
        next.pop_back();
    }

    next.push_back(std::move(screen));
    apply(std::move(next), transition);
}

void NavigationStack::setScreens(std::vector<std::shared_ptr<Screen>> next, const NavigationTransition& transition) {
    checkOpen();
    apply(std::move(next), transition);
}

// The screens leave from the top down, each one as it would leave through a pop.
void NavigationStack::close() {
    if (closed) {
        return;
    }

    finish();
    closed = true;
    std::vector<std::shared_ptr<Screen>> leaving = std::exchange(screens, {});
    for (auto screen = leaving.rbegin(); screen != leaving.rend(); ++screen) {
        release(*screen);
    }

    changed.clear();
    navigation.forget(*this);
}

void NavigationStack::checkOpen() const {
    if (closed) {
        throw std::logic_error("The navigation stack is closed.");
    }
}

void NavigationStack::checkJoining(const std::vector<std::shared_ptr<Screen>>& next) const {
    for (const std::shared_ptr<Screen>& screen : next) {
        if (!screen) {
            throw std::invalid_argument("A navigation stack holds screens, and one of them is missing.");
        }
        if (std::ranges::count(next, screen) > 1) {
            throw std::invalid_argument("A navigation stack holds each screen once.");
        }

        const std::shared_ptr<NavigationStack> holder = screen->getStack();
        if ((holder && holder.get() != this) || screen->getPresentation()) {
            throw std::invalid_argument("The screen already shows in another navigation stack or in a presentation.");
        }
    }
}

bool NavigationStack::isActive() const {
    return shown && !closed && navigation.isHostShowing(*this);
}

void NavigationStack::setShown(bool value) {
    if (shown == value) {
        return;
    }

    finish();
    const bool before = isActive();
    shown = value;
    const std::shared_ptr<Screen> top = getTop();
    if (!top || before == isActive()) {
        return;
    }

    if (value) {
        navigation.showAtOnce(*top);
        return;
    }

    navigation.hideAtOnce(*top);
}

// The screens that join take their place first, and the hooks and then the handlers of the change run once the list is whole, so one that changes the stack finds it in order and finishes this change before its own.
void NavigationStack::apply(std::vector<std::shared_ptr<Screen>> next, const NavigationTransition& transition) {
    checkJoining(next);
    finish();

    const std::shared_ptr<Screen> outgoing = getTop();
    const std::shared_ptr<Screen> incoming = next.empty() ? nullptr : next.back();
    std::vector<std::shared_ptr<Screen>> removed;
    std::vector<std::shared_ptr<Screen>> joined;
    for (const std::shared_ptr<Screen>& screen : screens) {
        if (std::ranges::find(next, screen) == next.end()) {
            removed.push_back(screen);
        }
    }

    for (const std::shared_ptr<Screen>& screen : next) {
        if (std::ranges::find(screens, screen) == screens.end()) {
            joined.push_back(screen);
        }
    }

    const bool backward = incoming && incoming != outgoing && std::ranges::find(screens, incoming) != screens.end();
    screens = std::move(next);
    ++generation;
    const std::uint64_t started = generation;
    const std::shared_ptr<NavigationStack> self = shared_from_this();
    for (const std::shared_ptr<Screen>& screen : joined) {
        navigation.place(*screen, self);
    }

    const bool moves = outgoing != incoming;
    change = Change{.outgoing = moves ? outgoing : nullptr, .incoming = moves ? incoming : nullptr, .removed = std::move(removed), .transition = transition.reduced(navigation.isMotionReduced()), .backward = backward};
    if (isActive() && change->outgoing) {
        navigation.beginDisappear(*change->outgoing);
    }

    if (generation == started && change && isActive() && change->incoming) {
        navigation.beginAppear(*change->incoming);
    }

    // A hook that changed the stack told the handlers of its own change, while one that only showed or hid the stack finished this change, which the handlers still hear.
    if (generation != started) {
        return;
    }

    changed.emit(*this);

    // A handler of the change that changed the stack or showed or hid it finished this change already.
    if (generation != started || !change) {
        return;
    }

    if (!isActive() || change->transition.isInstant() || !change->incoming || !change->outgoing) {
        finish();
    }
}

void NavigationStack::finish() {
    if (!change) {
        return;
    }

    Change ended = std::move(*change);
    change.reset();

    if (ended.outgoing && ended.outgoing->getState() == Screen::State::Disappearing) {
        navigation.endDisappear(*ended.outgoing);
    }

    if (ended.incoming && ended.incoming->getState() == Screen::State::Appearing) {
        navigation.endAppear(*ended.incoming);
    }

    for (const std::shared_ptr<Screen>& screen : ended.removed) {
        release(screen);
    }
}

// A screen that still shows hears that it leaves before it is removed.
void NavigationStack::release(const std::shared_ptr<Screen>& screen) {
    navigation.hideAtOnce(*screen);
    navigation.removeScreen(*screen);
}

void NavigationStack::advance(float deltaSeconds) {
    if (!change) {
        return;
    }

    if (!change->moving) {
        change->moving = navigation.isReady(*change->incoming) || change->waited >= Navigation::kReadyWait;
        change->waited += deltaSeconds;
        if (!change->moving) {
            return;
        }
    }

    change->elapsed += deltaSeconds;
    if (change->elapsed >= change->transition.duration) {
        finish();
    }
}

float NavigationStack::getProgress() const {
    if (!change || change->transition.duration <= 0.0F) {
        return 1.0F;
    }

    const float linear = std::clamp(change->elapsed / change->transition.duration, 0.0F, 1.0F);
    return change->transition.ease.apply(linear);
}

// The new top of the stack shows at once, without a transition.
std::vector<std::shared_ptr<Screen>> NavigationStack::detach(std::size_t index) {
    finish();
    const auto first = std::next(screens.begin(), static_cast<std::ptrdiff_t>(std::min(index, screens.size())));
    std::vector<std::shared_ptr<Screen>> moved(first, screens.end());
    screens.erase(first, screens.end());

    const std::shared_ptr<Screen> top = getTop();
    if (top && isActive()) {
        navigation.showAtOnce(*top);
    }

    changed.emit(*this);
    return moved;
}

// The top screen that the moved screens cover disappears at once, and the moved top shows when the stack does.
void NavigationStack::attach(std::vector<std::shared_ptr<Screen>> moved) {
    finish();
    if (moved.empty()) {
        return;
    }

    const std::shared_ptr<Screen> covered = getTop();
    if (covered) {
        navigation.hideAtOnce(*covered);
    }

    const std::shared_ptr<NavigationStack> self = shared_from_this();
    for (std::shared_ptr<Screen>& screen : moved) {
        screens.push_back(std::move(screen));
        navigation.place(*screens.back(), self);
    }

    const std::shared_ptr<Screen> top = getTop();
    if (isActive()) {
        navigation.showAtOnce(*top);
    } else {
        navigation.hideAtOnce(*top);
    }

    changed.emit(*this);
}

} // namespace haylen::ui
