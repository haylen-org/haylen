#pragma once

#include <array>
#include <string_view>
#include <utility>

#include "haylen/input/Key.hpp"

namespace haylen::platform::worker {

// The keys of the engine by the `code` of the keyboard events of the browser, which names a key by its place on a US keyboard whatever the layout, as the engine names keys.
class Keys final {
  public:
    // Returns the key of a code, or `Key::Unknown` for a code that names no key of the engine, such as a media key.
    [[nodiscard]] static input::Key fromCode(std::string_view code) noexcept;

  private:
    static const std::array<std::pair<std::string_view, input::Key>, 118> kCodes;
};

} // namespace haylen::platform::worker
