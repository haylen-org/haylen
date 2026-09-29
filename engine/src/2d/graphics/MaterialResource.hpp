#pragma once

#include <cstdint>
#include <functional>
#include <map>
#include <string>
#include <vector>

#include "haylen/graphics/Shader.hpp"
#include "haylen/graphics/Texture.hpp"

namespace haylen::graphics2d {

// The shader and the values behind a material handle. Values stay by name, so a reloaded shader takes the ones that still fit its uniforms, and the revision grows with every change so the renderer copies the values once per change.
struct MaterialResource {
    graphics::Shader shader;
    std::map<std::string, std::vector<float>, std::less<>> values;
    std::map<std::string, graphics::Texture, std::less<>> textures;
    std::uint32_t revision = 0;

    // Appends the uniform blocks of the shader one after another, each at its size, with the values that fit their uniforms and zeros elsewhere.
    void pack(std::vector<std::uint8_t>& bytes) const;

    // Returns the texture set for a view slot of the shader, or an empty texture.
    [[nodiscard]] graphics::Texture getTextureAt(int slot) const;
};

} // namespace haylen::graphics2d
