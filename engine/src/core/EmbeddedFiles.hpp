#pragma once

#include <cstdint>
#include <span>

namespace haylen::core {

// Files compiled into the engine, so it can draw text before any package loads. The build generates the method bodies.
class EmbeddedFiles final {
  public:
    // Roboto Medium, shipped with Dear ImGui under the Apache 2.0 license.
    [[nodiscard]] static std::span<const std::uint8_t> getDefaultFont();

    // Space Mono Regular, under the SIL Open Font License 1.1 that `engine/fonts/space-mono-license.txt` holds, which draws the monospace text of the UI.
    [[nodiscard]] static std::span<const std::uint8_t> getMonospaceFont();

    // The Thai phrase model of BudouX, a JSON file under the Apache 2.0 license.
    [[nodiscard]] static std::span<const std::uint8_t> getThaiPhraseModel();
};

} // namespace haylen::core
