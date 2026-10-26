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

    // The shader packages of the programs of the 2D renderer, which the Haylen shader compiler writes when the engine builds.
    [[nodiscard]] static std::span<const std::uint8_t> getSpriteShader();
    [[nodiscard]] static std::span<const std::uint8_t> getTextShader();
    [[nodiscard]] static std::span<const std::uint8_t> getMeshShader();
    [[nodiscard]] static std::span<const std::uint8_t> getBlendShader();
    [[nodiscard]] static std::span<const std::uint8_t> getCompositeShader();
    [[nodiscard]] static std::span<const std::uint8_t> getLightShader();
    [[nodiscard]] static std::span<const std::uint8_t> getMetaballShader();
    [[nodiscard]] static std::span<const std::uint8_t> getRecolorShader();
    [[nodiscard]] static std::span<const std::uint8_t> getEffectShader();
    [[nodiscard]] static std::span<const std::uint8_t> getShapeShader();
    [[nodiscard]] static std::span<const std::uint8_t> getBackdropShader();

    // The shader package of the conformance tests of the GPU devices, which only `haylen-gpu-tests` embeds.
    [[nodiscard]] static std::span<const std::uint8_t> getConformanceShader();
};

} // namespace haylen::core
