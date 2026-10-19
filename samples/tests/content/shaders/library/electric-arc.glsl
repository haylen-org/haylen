// Shades a stroke as a jagged electric arc that jumps to a new shape many times a second, with a white core and a glow.
@include haylen/material.glsl
@include common.glsl

@fs electricArc_fs
@include_block haylen_fragment
@include_block library_common
layout(binding=1) uniform electricArc_params {
    vec4 arc_color;
    float jitter;
    float thickness;
    float time;
};

void main() {
    vec2 local = library_local(uv);
    float jump = floor(time * 18.0);
    float offset = (library_noise(vec2(local.x * 8.0, jump)) - 0.5) * jitter + (library_noise(vec2(local.x * 30.0, jump + 7.0)) - 0.5) * jitter * 0.4;
    float distance = abs(local.y - 0.5 - offset);
    float core = 1.0 - smoothstep(thickness * 0.3, thickness, distance);
    float glow = exp(-distance * 7.0);
    haylen_output(vec4(mix(arc_color.rgb, vec3(1.0), core), clamp(core + glow * 0.8, 0.0, 1.0) * color.a * arc_color.a));
}
@end

@program electricArc haylen_vs electricArc_fs
