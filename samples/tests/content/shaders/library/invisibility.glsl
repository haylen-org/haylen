// Hides a sprite but its shimmering edges, like a cloak of invisibility, by the amount.
@include haylen/material.glsl
@include common.glsl

@fs invisibility_fs
@include_block haylen_fragment
@include_block library_common
layout(binding=1) uniform invisibility_params {
    vec2 texel;
    float amount;
    float time;
};

void main() {
    vec4 base = haylen_base(uv);
    float right = haylen_texture(haylen_inside(uv + vec2(texel.x * 2.0, 0.0))).a;
    float left = haylen_texture(haylen_inside(uv - vec2(texel.x * 2.0, 0.0))).a;
    float down = haylen_texture(haylen_inside(uv + vec2(0.0, texel.y * 2.0))).a;
    float up = haylen_texture(haylen_inside(uv - vec2(0.0, texel.y * 2.0))).a;
    float edge = clamp(abs(right - left) + abs(down - up), 0.0, 1.0);
    float shimmer = library_noise(library_local(uv) * 20.0 + time * 2.0);
    haylen_output(vec4(mix(base.rgb, vec3(0.85, 0.95, 1.0), amount * 0.5), mix(base.a, edge * 0.8 + base.a * 0.08 * shimmer, amount)));
}
@end

@program invisibility haylen_vs invisibility_fs
