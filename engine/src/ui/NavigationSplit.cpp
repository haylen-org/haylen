#include "haylen/ui/NavigationSplit.hpp"

#include <algorithm>
#include <iterator>
#include <stdexcept>
#include <utility>
#include <vector>

#include "haylen/ui/Navigation.hpp"
#include "haylen/ui/NavigationStack.hpp"
#include "haylen/ui/Screen.hpp"

namespace haylen::ui {

NavigationSplit::NavigationSplit(Navigation& owner, Region primaryRegion, Region detailRegion, std::shared_ptr<Screen> root, Factory placeholderFactory, bool startCollapsed) : navigation(owner), placeholder(std::move(placeholderFactory)), collapsed(startCollapsed) {
    if (!root) {
        throw std::invalid_argument("A navigation split needs the root screen of its list.");
    }

    primary = navigation.createStack(std::move(primaryRegion), std::move(root));
    detail = navigation.createStack(std::move(detailRegion));
    primaryChanged = primary->changed.connect([this](NavigationStack&) { followPrimary(); });
    if (!collapsed) {
        showPlaceholder();
    }
}

NavigationSplit::~NavigationSplit() = default;

void NavigationSplit::checkOpen() const {
    if (closed) {
        throw std::logic_error("The navigation split is closed.");
    }
}

void NavigationSplit::followPrimary() {
    if (detailStart && primary->size() <= *detailStart) {
        detailStart.reset();
    }
}

void NavigationSplit::showDetail(std::shared_ptr<Screen> screen, const std::optional<NavigationTransition>& transition) {
    checkOpen();
    if (collapsed) {
        const std::size_t start = detailStart.value_or(primary->size());
        const std::vector<std::shared_ptr<Screen>>& list = primary->getScreens();
        std::vector<std::shared_ptr<Screen>> next(list.begin(), std::next(list.begin(), static_cast<std::ptrdiff_t>(start)));
        next.push_back(std::move(screen));
        primary->setScreens(std::move(next), transition.value_or(NavigationTransition{}));
        detailStart = start;
        return;
    }

    placeholderScreen.reset();
    detail->setScreens({std::move(screen)}, transition.value_or(NavigationTransition{.effect = NavigationTransition::Effect::None}));
}

void NavigationSplit::clearDetail(const std::optional<NavigationTransition>& transition) {
    checkOpen();
    if (collapsed) {
        if (!detailStart) {
            return;
        }

        const std::vector<std::shared_ptr<Screen>>& list = primary->getScreens();
        primary->setScreens({list.begin(), std::next(list.begin(), static_cast<std::ptrdiff_t>(*detailStart))}, transition.value_or(NavigationTransition{}));
        return;
    }

    if (!hasDetail()) {
        return;
    }

    detail->setScreens({}, transition.value_or(NavigationTransition{.effect = NavigationTransition::Effect::None}));
    showPlaceholder();
}

bool NavigationSplit::hasDetail() const noexcept {
    if (collapsed) {
        return detailStart.has_value();
    }

    return !detail->empty() && detail->getTop() != placeholderScreen;
}

// Collapsing moves the detail screens on top of the list, and expanding moves them back, with their GUIs and their state. The placeholder only shows in the detail area.
void NavigationSplit::setCollapsed(bool value) {
    checkOpen();
    if (collapsed == value) {
        return;
    }

    collapsed = value;
    if (value) {
        dropPlaceholder();
        std::vector<std::shared_ptr<Screen>> moved = detail->detach(0);
        if (moved.empty()) {
            return;
        }

        const std::size_t start = primary->size();
        primary->attach(std::move(moved));
        detailStart = start;
        return;
    }

    if (!detailStart) {
        showPlaceholder();
        return;
    }

    const std::size_t start = *detailStart;
    detailStart.reset();
    detail->attach(primary->detach(start));
}

void NavigationSplit::showPlaceholder() {
    if (!placeholder || !detail->empty()) {
        return;
    }

    std::shared_ptr<Screen> screen = placeholder();
    if (!screen) {
        throw std::invalid_argument("The placeholder of a navigation split made no screen.");
    }

    placeholderScreen = screen;
    detail->push(std::move(screen), {.effect = NavigationTransition::Effect::None});
}

void NavigationSplit::dropPlaceholder() {
    if (!placeholderScreen || detail->getTop() != placeholderScreen) {
        placeholderScreen.reset();
        return;
    }

    placeholderScreen.reset();
    detail->setScreens({}, {.effect = NavigationTransition::Effect::None});
}

void NavigationSplit::close() {
    if (closed) {
        return;
    }

    closed = true;
    primaryChanged.disconnect();
    primary->close();
    detail->close();
    navigation.forget(*this);
}

} // namespace haylen::ui
