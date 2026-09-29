#pragma once

#include <string>
#include <vector>

#include "haylen/input/Key.hpp"
#include "haylen/platform/Event.hpp"

namespace haylen::platform {

// Turns the edits of the plain on-screen keyboard, which has no text field of its own, into the key and character events of a physical keyboard. Text inside an open composition types nothing until the input method commits it.
class KeyboardTranslator final {
  public:
    // Starts from an empty native field, as the keyboard has when it opens.
    void reset() noexcept;

    [[nodiscard]] std::vector<Event> translate(const Event& event);

  private:
    static void press(std::vector<Event>& events, input::Key key);

    std::u32string committed;
};

} // namespace haylen::platform
