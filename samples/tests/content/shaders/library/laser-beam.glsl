// Shades a stroke as a laser beam: a hard white core, a saturated band of the beam color around it, a wide soft halo and streaks of energy that race along it, with ends that fade in, for beams, rays and sniper shots drawn with the additive blend.
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

    // Pulses of energy race along the beam and swell its band and halo as they pass.
    float pulses = pow(0.5 + 0.5 * sin(local.x * 34.0 - time * speed), 6.0);
    float surge = 0.85 + 0.45 * pulses + 0.15 * library_noise(vec2(local.x * 60.0 - time * speed * 1.7, time));
    float window = 1.0 - smoothstep(0.65, 1.0, across);
    float core = 1.0 - smoothstep(width * 0.16, width * 0.28, across);
    float band = exp(-across / max(width * 0.5, 0.01)) * surge;
    float halo = exp(-across * 2.4) * 0.5 * surge * window;

    // The beam shimmers a little over time and fades in at both ends.
    float shimmer = 0.92 + 0.08 * sin(time * 47.0);
    float ends = smoothstep(0.0, 0.04, local.x) * smoothstep(1.0, 0.97, local.x);

    vec3 light = (vec3(1.0) * core * 1.5 + beam_color.rgb * (band * 1.3 + halo)) * shimmer * ends;
    float amount = clamp(max(max(light.r, light.g), light.b), 0.0, 1.0);
    haylen_output(vec4(light / max(amount, 0.0001), amount * color.a * beam_color.a));
}
@end

@program laserBeam haylen_vs laserBeam_fs
