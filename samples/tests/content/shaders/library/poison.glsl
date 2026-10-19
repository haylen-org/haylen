// Tints a sprite a sick green that pulses, with bubbles that rise through it.
@include haylen/material.glsl
@include common.glsl

@fs poison_fs
@include_block haylen_fragment
@include_block library_common
layout(binding=1) uniform poison_params {
    float amount;
    float time;
};

void main() {
    vec4 base = haylen_base(uv);
    vec2 local = library_local(uv);
    float bubbles = smoothstep(0.75, 0.8, library_noise(vec2(local.x * 10.0, local.y * 10.0 + time * 2.0)));
    float pulse = 0.5 + 0.5 * sin(time * 4.0);
    vec3 green = vec3(0.35, 0.95, 0.3) * (library_luminance(base.rgb) * 0.7 + 0.3);
    haylen_output(vec4(mix(base.rgb, green, amount * (0.6 + 0.4 * pulse)) + vec3(0.6, 1.0, 0.4) * bubbles * amount * 0.6, base.a));
}
@end

@program poison haylen_vs poison_fs
