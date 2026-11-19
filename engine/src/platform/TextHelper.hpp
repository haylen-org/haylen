#pragma once

#include <string>
#include <string_view>

namespace haylen::platform {

// Prepares the text of the engine and of the app for the APIs of the systems.
class TextHelper final {
  public:
    // Many APIs take only well-formed UTF-8, and a Lua string may hold any bytes, so each sequence that is not UTF-8 becomes U+FFFD, the character that the engine decodes and draws for it.
    [[nodiscard]] static std::string toValidUtf8(std::string_view text);
};

} // namespace haylen::platform
