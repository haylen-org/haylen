#include "haylen/ui/NavigationTabs.hpp"

#include <algorithm>
#include <format>
#include <stdexcept>
#include <utility>

#include "haylen/ui/Navigation.hpp"
#include "haylen/ui/NavigationStack.hpp"
#include "haylen/ui/Screen.hpp"

namespace haylen::ui {

NavigationTabs::NavigationTabs(Navigation& owner, Region value, std::vector<Tab> list, std::string_view initial) : navigation(owner), region(std::move(value)) {
    if (list.empty()) {
        throw std::invalid_argument("Navigation tabs need at least one tab.");
    }

    for (Tab& tab : list) {
        if (tab.id.empty()) {
            throw std::invalid_argument("Every navigation tab needs an id.");
        }
        if (!tab.root) {
            throw std::invalid_argument(std::format("The navigation tab \"{}\" needs a root.", tab.id));
        }
        if (std::ranges::any_of(entries, [&](const Entry& entry) { return entry.tab.id == tab.id; })) {
            throw std::invalid_argument(std::format("The navigation tab \"{}\" is given more than once.", tab.id));
        }

        entries.push_back({.tab = std::move(tab), .stack = nullptr});
    }

    selected = initial.empty() ? entries.front().tab.id : std::string(initial);
    static_cast<void>(find(selected));
}

NavigationTabs::~NavigationTabs() = default;

void NavigationTabs::checkOpen() const {
    if (closed) {
        throw std::logic_error("The navigation tabs are closed.");
    }
}

NavigationTabs::Entry& NavigationTabs::find(std::string_view id) {
    const auto found = std::ranges::find(entries, id, [](const Entry& entry) -> const std::string& { return entry.tab.id; });
    if (found == entries.end()) {
        throw std::invalid_argument(std::format("There is no navigation tab with the id \"{}\".", id));
    }

    return *found;
}

// The stack of a tab that does not show is hidden before its root joins, so the root builds its GUI only once its tab shows.
const std::shared_ptr<NavigationStack>& NavigationTabs::open(Entry& entry) {
    if (entry.stack) {
        return entry.stack;
    }

    std::shared_ptr<Screen> root = entry.tab.root();
    if (!root) {
        throw std::invalid_argument(std::format("The root of the navigation tab \"{}\" made no screen.", entry.tab.id));
    }

    std::shared_ptr<NavigationStack> stack = navigation.createStack(region);
    stack->setShown(entry.tab.id == selected);
    stack->push(std::move(root), {.effect = NavigationTransition::Effect::None});
    entry.stack = std::move(stack);
    return entry.stack;
}

void NavigationTabs::select(std::string_view id) {
    checkOpen();
    Entry& entry = find(id);
    if (entry.tab.id == selected) {
        open(entry)->popToRoot();
        return;
    }

    const std::string previous = std::exchange(selected, entry.tab.id);
    find(previous).stack->setShown(false);
    open(entry)->setShown(true);
    selectionChanged.emit(selected, previous);
}

const std::shared_ptr<NavigationStack>& NavigationTabs::getStack(std::string_view id) {
    checkOpen();
    return open(find(id));
}

const std::shared_ptr<NavigationStack>& NavigationTabs::getCurrent() {
    return getStack(selected);
}

bool NavigationTabs::goBackToFirst() {
    if (closed || entries.front().tab.id == selected) {
        return false;
    }

    select(entries.front().tab.id);
    return true;
}

void NavigationTabs::close() {
    if (closed) {
        return;
    }

    closed = true;
    for (const Entry& entry : entries) {
        if (entry.stack) {
            entry.stack->close();
        }
    }

    navigation.forget(*this);
}

} // namespace haylen::ui
