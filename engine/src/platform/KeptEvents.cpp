#include "platform/KeptEvents.hpp"

#include <algorithm>
#include <utility>

namespace haylen::platform {

void KeptEvents::keep(Event event) {
    const auto sameName = [&event](const Event& kept) { return kept.name == event.name; };
    if (static_cast<std::size_t>(std::ranges::count_if(events, sameName)) == kLimitPerName) {
        events.erase(std::ranges::find_if(events, sameName));
    }

    events.push_back(std::move(event));
}

std::vector<KeptEvents::Event> KeptEvents::take() {
    return std::exchange(events, {});
}

std::size_t KeptEvents::size() const noexcept {
    return events.size();
}

} // namespace haylen::platform
