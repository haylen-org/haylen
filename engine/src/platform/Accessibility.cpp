#include "haylen/platform/Accessibility.hpp"

#include <algorithm>
#include <stdexcept>

#include "haylen/core/EventBus.hpp"
#include "haylen/core/LifecycleEvent.hpp"
#include "platform/Host.hpp"

namespace haylen::platform {

Accessibility::Accessibility(Host& owner, core::EventBus& bus) : host(owner), events(bus), settings(owner.getAccessibilitySettings()), treeWanted(owner.isAccessibilityTreeWanted()) {}

void Accessibility::publish(std::span<const AccessibilityElement> tree, std::uint64_t focusedElement) {
    if (focusedElement == focused && std::ranges::equal(tree, elements)) {
        return;
    }
    elements.assign(tree.begin(), tree.end());
    focused = focusedElement;
    host.updateAccessibility(elements, focused);
}

void Accessibility::announce(std::string_view text, bool interrupt) {
    if (text.empty()) {
        throw std::invalid_argument("An announcement needs a text.");
    }
    host.announce(text, interrupt);
}

void Accessibility::perform(const Request& request) {
    const std::scoped_lock lock(mutex);
    requests.push_back(request);
}

void Accessibility::takeRequests(std::vector<Request>& taken) {
    taken.clear();
    const std::scoped_lock lock(mutex);
    taken.swap(requests);
}

void Accessibility::pump() {
    const AccessibilitySettings reported = host.getAccessibilitySettings();
    if (reported != settings) {
        settings = reported;
        events.emit(core::LifecycleEvent::kAccessibilitySettingsChanged, settings.toJson());
    }
    treeWanted = host.isAccessibilityTreeWanted();
}

} // namespace haylen::platform
