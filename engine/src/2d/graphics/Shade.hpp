#pragma once

#include <cstdint>

namespace haylen::graphics {
struct TextureResource;
}

namespace haylen::graphics2d {

struct MaterialResource;

// How the draws of a command are shaded: the material with a copy of the values it had when they were made, and in lit canvases how they take light.
struct Shade {
    MaterialResource* material = nullptr;
    std::uint32_t revision = 0;
    std::uint32_t version = 0;

    // Where the copy of the material starts in the uniform bytes and the textures of the frame.
    std::uint32_t uniformBegin = 0;
    std::uint32_t textureBegin = 0;

    graphics::TextureResource* normalMap = nullptr;
    float specular = 0.0F;
    float shininess = 0.0F;
    float emission = 0.0F;
    std::uint8_t lightMask = 1;
    std::int16_t layer = 0;
    bool unshaded = false;

    // Tells whether draws of both shades can share a command, which takes the same material values and the same lighting.
    [[nodiscard]] bool matches(const Shade& other) const noexcept {
        return material == other.material && revision == other.revision && version == other.version && normalMap == other.normalMap && specular == other.specular && shininess == other.shininess && emission == other.emission && lightMask == other.lightMask && layer == other.layer && unshaded == other.unshaded;
    }
};

} // namespace haylen::graphics2d
