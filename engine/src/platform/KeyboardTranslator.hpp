#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "haylen/input/Key.hpp"
#include "haylen/platform/Event.hpp"

namespace haylen::platform {

// Turns the edits of the plain on-screen keyboard, which has no text field of its own, into the key and character events of a physical keyboard. Every plain keyboard that opens has a revision of its own and starts from an empty native field. Text inside an open composition types nothing until the input method commits it.
class KeyboardTranslator final {
  public:
    [[nodiscard]] std::vector<Event> translate(const Event& event);

  private:
    static void press(std::vector<Event>& events, input::Key key);

    std::u32string committed;
    std::uint64_t revision = 0;
};

} // namespace haylen::platform
