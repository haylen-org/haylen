#pragma once

#include <cstdint>
#include <span>

namespace haylen::core {

// Files compiled into the engine, so it can draw text before any package loads. The build generates the method bodies.
class EmbeddedFiles final {
  public:
    // Roboto Medium, shipped with Dear ImGui under the Apache 2.0 license.
    [[nodiscard]] static std::span<const std::uint8_t> getDefaultFont();
};

} // namespace haylen::core
