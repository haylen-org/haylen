#pragma once

#include <optional>

#include "platform/SystemFont.hpp"

namespace haylen::platform {

// Finds the faces of the system that draw characters through the font fallback of DirectWrite, in the language of the person, and names each face by its file and its index in the file.
class WindowsFonts final {
  public:
    [[nodiscard]] static std::optional<SystemFont> find(char32_t codePoint);
};

} // namespace haylen::platform
