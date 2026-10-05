// Draws a sprite that dissolves into noise with a colored edge, with an outline and a glow around its visible pixels. The record after the sprite in the instance stream holds its effect, and the quad grows by the reach of the outline and the glow so they can spill past the sprite.
@include haylen/material.glsl

@vs effect_vs
@include_block haylen_vertex
in vec4 effect_dissolve_color;
in vec4 effect_outline_color;
in vec4 effect_glow_color;
in vec4 effect_amounts;
in vec2 effect_reach;
in vec2 effect_widths;
out vec4 dissolve_color;
out vec4 outline_color;
out vec4 glow_color;
out vec4 amounts;
out vec2 widths;

// Places the corners of the grown quad like the sprite program places its own, with the flips of the sprite, and hands the effect to the fragment stage.
void main() {
    haylen_output_premultiply = haylen_premultiply;
    vec2 grown = corner * (1.0 + 2.0 * effect_reach) - effect_reach;
    vec2 local = (grown - instance_pivot) * instance_size;
    float sine = sin(instance_rotation);
    float cosine = cos(instance_rotation);
    vec2 world = instance_position + vec2(local.x * cosine - local.y * sine, local.x * sine + local.y * cosine);
    gl_Position = view_projection * vec4(world, 0.0, 1.0);

    float flags = floor(instance_parameters.x * 255.0 + 0.5);
    vec2 texel = grown;
    if (mod(flags, 2.0) >= 1.0) {
        texel.x = 1.0 - texel.x;
    }
    if (mod(floor(flags / 2.0), 2.0) >= 1.0) {
        texel.y = 1.0 - texel.y;
    }
    if (mod(floor(flags / 4.0), 2.0) >= 1.0) {
        texel = texel.yx;
    }
    uv = mix(instance_uv.xy, instance_uv.zw, texel);
    haylen_source = vec4(min(instance_uv.xy, instance_uv.zw), max(instance_uv.xy, instance_uv.zw));
    color = instance_color;
    haylen_flash = instance_flash;
    haylen_text_style = vec3(0.0);
    haylen_transform = vec2(instance_rotation, flags);

    dissolve_color = effect_dissolve_color;
    outline_color = effect_outline_color;
    glow_color = effect_glow_color;
    amounts = effect_amounts;
    widths = effect_widths;
}
@end

@fs effect_fs
@include_block haylen_fragment
in vec4 dissolve_color;
in vec4 outline_color;
in vec4 glow_color;
in vec4 amounts;
in vec2 widths;

// The texture inside the source of the sprite, and nothing outside it, where the outline and the glow spill.
vec4 effect_sample(vec2 point) {
    bool inside = all(greaterThanEqual(point, haylen_source.xy)) && all(lessThanEqual(point, haylen_source.zw));
    return inside ? haylen_texture(point) : vec4(0.0);
}

float effect_hash(vec2 cell) {
    return fract(sin(dot(cell, vec2(127.1, 311.7))) * 43758.5453);
}

// Smooth value noise from 0 to 1 over cells of one unit.
float effect_noise(vec2 point) {
    vec2 cell = floor(point);
    vec2 inside = fract(point);
    inside = inside * inside * (3.0 - 2.0 * inside);
    float top = mix(effect_hash(cell), effect_hash(cell + vec2(1.0, 0.0)), inside.x);
    float bottom = mix(effect_hash(cell + vec2(0.0, 1.0)), effect_hash(cell + vec2(1.0, 1.0)), inside.x);
    return mix(top, bottom, inside.y);
}

// Amounts: x is the dissolve, y the share of its edge and z the size of its noise cells over 64 pixels. Widths: x is the outline width and y the glow size in pixels.
void main() {
    vec2 texel = 1.0 / vec2(textureSize(sampler2D(sprite_texture, sprite_sampler), 0));
    vec4 base = effect_sample(uv) * color;
    base.rgb = mix(base.rgb, haylen_flash.rgb, haylen_flash.a);

    float dissolve = amounts.x;
    if (dissolve > 0.0) {
        float noise = effect_noise(uv / texel / max(amounts.z * 64.0, 1.0));
        // The threshold runs from just below the noise to above it, so the edge band appears with the dissolve and everything is gone at 1.
        float edge = amounts.y;
        float threshold = dissolve * (1.0 + edge) - edge;
        if (noise < threshold) {
            base.a = 0.0;
        } else if (noise < threshold + edge) {
            base.rgb = mix(base.rgb, dissolve_color.rgb, dissolve_color.a);
        }
    }

    // The outline takes the most covered of twelve points around each pixel at its width, and the glow the average of two rings of them.
    float around = 0.0;
    float glow = 0.0;
    for (int index = 0; index < 12; ++index) {
        float angle = float(index) * 0.5235988;
        vec2 direction = vec2(cos(angle), sin(angle)) * texel;
        if (widths.x > 0.0) {
            around = max(around, effect_sample(uv + direction * widths.x).a);
        }
        if (widths.y > 0.0) {
            glow += effect_sample(uv + direction * widths.y).a + effect_sample(uv + direction * widths.y * 0.5).a;
        }
    }
    float fading = 1.0 - dissolve;
    vec4 under = vec4(glow_color.rgb, glow_color.a * min(glow / 12.0, 1.0) * fading);
    float lining = outline_color.a * around * fading;
    under = vec4(mix(under.rgb, outline_color.rgb, lining / max(lining + under.a * (1.0 - lining), 0.0001)), lining + under.a * (1.0 - lining));

    float alpha = base.a + under.a * (1.0 - base.a);
    vec3 mixed = (base.rgb * base.a + under.rgb * under.a * (1.0 - base.a)) / max(alpha, 0.0001);
    haylen_output(vec4(mixed, alpha));
}
@end

@program effect effect_vs effect_fs
