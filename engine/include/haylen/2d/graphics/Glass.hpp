#pragma once

#include "haylen/math/Color.hpp"

namespace haylen::graphics2d {

// How a glass draw shows what its canvas and the canvases before it drew behind its shape this frame: blurred over a radius, with its saturation and brightness changed, a tint mixed over it, a rim of light along its edge and a bend near the edge like the rim of a lens. Distances are in units of the canvas. A glass with every value at its default shows the image behind it blurred and nothing else.
struct Glass {
    // Blurs the image behind over this radius, where 0 keeps it sharp.
    float blur = 16.0F;

    // Scales the colors away from their gray, where 0 makes them gray and values above 1 make them richer.
    float saturation = 1.0F;

    // Multiplies the colors, where values above 1 make the glass lighter.
    float brightness = 1.0F;

    // Mixes this color over the image behind by its alpha, such as a white haze of frosted glass.
    math::Color tint = math::Color::transparent();

    // Brightens a band along the edge as if light caught it, strongest where the edge faces the top-left, from 0 for none to 1.
    float rim = 0.0F;

    // The width of the band of the rim.
    float rimWidth = 2.0F;

    // Bends the image near the edge, which shows what lies up to this far past the edge at the edge itself, fading inward over three times the distance.
    float refraction = 0.0F;
};

} // namespace haylen::graphics2d
