#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "haylen/platform/Bridge.hpp"
#include "varn/dataview/DataView.h"

namespace haylen::platform {

// The events that native code sends while no app runs, which the next app receives in the order they came. Each name keeps its newest events up to a limit, so a plugin that goes on sending while no app runs never fills the memory.
class KeptEvents final {
  public:
    // An event with its JSON, the byte buffers it refers to and the data views that its tokens name, which it keeps alive until the bridge took its own references.
    struct Event {
        std::string name;
        std::string json;
        std::vector<std::vector<std::byte>> buffers;
        std::vector<varn::dataview::DataView> views;
        Bridge::EmitOptions options;
    };

    static constexpr std::size_t kLimitPerName = 32;

    // Keeps the event, and drops the oldest event of its name once the name holds the limit.
    void keep(Event event);

    // Returns the kept events in the order they came, and keeps none.
    [[nodiscard]] std::vector<Event> take();

    [[nodiscard]] std::size_t size() const noexcept;

  private:
    std::vector<Event> events;
};

} // namespace haylen::platform
