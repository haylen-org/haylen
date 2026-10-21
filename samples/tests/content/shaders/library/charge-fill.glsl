// Fills a sprite clockwise from the top up to a share for cooldowns and charging abilities: the charged part keeps its colors under a shimmer of energy, the rest sinks into a dark cool shade, and the edge of the charge is a bright line of the fill color with a glow that trails behind it.
@include haylen/material.glsl
@include common.glsl

@fs chargeFill_fs
@include_block haylen_fragment
@include_block library_common
layout(binding=1) uniform chargeFill_params {
    vec4 fill_color;
    float progress;
    float glow;
    float time;
};

void main() {
    vec4 base = haylen_base(uv);
    vec2 offset = library_local(uv) - 0.5;

    // The share of a turn from the top, clockwise on screen, where the sprite counts as charged below the progress.
    float turn = fract(atan(offset.x, -offset.y) / 6.28318531 + 1.0);
    float charged = step(turn, progress);

    // The charged part shimmers with bands of energy that sweep around it, and the rest is dimmed and cooled.
    float bands = 0.5 + 0.5 * sin((turn * 18.0 - time * 3.0) * 6.28318531 * 0.25 + length(offset) * 14.0);
    vec3 lit = base.rgb + fill_color.rgb * bands * 0.12 * glow;
    vec3 dim = mix(vec3(library_luminance(base.rgb)), base.rgb, 0.25) * vec3(0.32, 0.36, 0.48);

    // The edge glows hard at the front of the charge and fades behind it.
    float behind = progress - turn;
    float edge = exp(-abs(behind) * 140.0) + step(0.0, behind) * exp(-behind * 14.0) * 0.45;
    float full = step(0.999, progress);
    vec3 shade = mix(dim, lit, charged) + fill_color.rgb * edge * glow * (1.0 - full) * (0.85 + 0.15 * sin(time * 20.0));
    haylen_output(vec4(min(shade, vec3(1.0)), base.a));
}
@end

@program chargeFill haylen_vs chargeFill_fs
