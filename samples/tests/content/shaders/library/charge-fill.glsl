// Fills a sprite clockwise from the top up to a share, with a glowing edge, and grays the rest, for cooldowns and charging abilities.
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
    vec2 local = library_local(uv) - 0.5;
    float angle = fract(atan(local.x, -local.y) / 6.2831853);
    float filled = step(angle, progress);
    float edge = exp(-abs(angle - progress) * 60.0) * glow * (0.8 + 0.2 * sin(time * 10.0));
    vec3 gray = vec3(library_luminance(base.rgb) * 0.5);
    haylen_output(vec4(mix(gray, mix(base.rgb, fill_color.rgb, 0.35), filled) + fill_color.rgb * edge, base.a));
}
@end

@program chargeFill haylen_vs chargeFill_fs
