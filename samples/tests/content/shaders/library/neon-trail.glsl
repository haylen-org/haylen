// Shades a stroke as a neon trail: a white core along its middle, a glow that fades toward its sides and a tail that fades toward its start, with a flicker that runs along it.
@include haylen/material.glsl
@include common.glsl

@fs neonTrail_fs
@include_block haylen_fragment
@include_block library_common
layout(binding=1) uniform neonTrail_params {
    vec4 glow_color;
    float core_width;
    float fade;
    float flicker;
    float time;
};

void main() {
    vec2 local = library_local(uv);
    float across = abs(local.y * 2.0 - 1.0);
    float core = 1.0 - smoothstep(core_width * 0.5, core_width, across);
    float glow = pow(1.0 - across, 2.2) * 0.9;
    float tail = pow(clamp(local.x, 0.0, 1.0), fade);
    float wobble = 1.0 - flicker * library_noise(vec2(local.x * 30.0 - time * 12.0, time * 3.0));
    vec3 shade = mix(glow_color.rgb, vec3(1.0), core);
    haylen_output(vec4(shade, clamp(core + glow, 0.0, 1.0) * tail * wobble * color.a * glow_color.a));
}
@end

@program neonTrail haylen_vs neonTrail_fs
