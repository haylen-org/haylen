#pragma once

#include <vector>

#include "haylen/2d/graphics/Material.hpp"
#include "haylen/math/Color.hpp"

namespace haylen::graphics2d {

// Full-screen adjustments applied when a world canvas is composited onto its destination. The materials run last, in order, each over the image the previous step made.
struct PostProcess {
    math::Color tint = math::Color::white();
    float saturation = 1.0F;
    float brightness = 1.0F;
    float contrast = 1.0F;
    float vignetteStrength = 0.0F;
    float vignetteRadius = 0.6F;
    float vignetteSoftness = 0.5F;
    math::Color fade = math::Color::transparent();
    std::vector<Material> materials;

    [[nodiscard]] bool operator==(const PostProcess&) const = default;
};

} // namespace haylen::graphics2d
