// Shades a stroke as a laser beam: a hard white core, a halo of the beam color and ripples that run along it.
@include haylen/material.glsl
@include common.glsl

@fs laserBeam_fs
@include_block haylen_fragment
@include_block library_common
layout(binding=1) uniform laserBeam_params {
    vec4 beam_color;
    float width;
    float speed;
    float time;
};

void main() {
    vec2 local = library_local(uv);
    float across = abs(local.y * 2.0 - 1.0);
    float ripple = 0.85 + 0.15 * sin(local.x * 60.0 - time * speed);
    float core = 1.0 - smoothstep(width * 0.25, width * 0.5, across);
    float halo = (1.0 - smoothstep(width * 0.5, 1.0, across)) * 0.6 * ripple;
    haylen_output(vec4(mix(beam_color.rgb, vec3(1.0), core), clamp(core + halo, 0.0, 1.0) * color.a * beam_color.a));
}
@end

@program laserBeam haylen_vs laserBeam_fs
