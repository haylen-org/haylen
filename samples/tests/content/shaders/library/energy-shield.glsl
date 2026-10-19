// Draws an energy shield over a square: a glowing rim, a honeycomb that a wave lights as it passes, and a faint fill, round whatever the image.
@include haylen/material.glsl
@include common.glsl

@fs energyShield_fs
@include_block haylen_fragment
@include_block library_common
layout(binding=1) uniform energyShield_params {
    vec4 shield_color;
    float rim;
    float hex_scale;
    float time;
};

void main() {
    vec2 local = library_local(uv) * 2.0 - 1.0;
    float radius = length(local);
    float inside = 1.0 - smoothstep(0.98, 1.0, radius);
    float edge = smoothstep(1.0 - rim, 1.0, radius);
    vec2 point = local * hex_scale;
    vec2 grid = vec2(1.0, 1.7320508);
    vec2 first = mod(point, grid) - grid * 0.5;
    vec2 second = mod(point - grid * 0.5, grid) - grid * 0.5;
    vec2 cell = abs(dot(first, first) < dot(second, second) ? first : second);
    float hexagon = smoothstep(0.42, 0.5, max(dot(cell, normalize(vec2(1.0, 1.7320508))), cell.x));
    float wave = 0.6 + 0.4 * sin(radius * 10.0 - time * 3.0);
    float alpha = (edge * 0.9 + hexagon * 0.35 * wave + 0.08) * inside;
    haylen_output(vec4(shield_color.rgb + vec3(edge * 0.5), alpha * shield_color.a));
}
@end

@program energyShield haylen_vs energyShield_fs
