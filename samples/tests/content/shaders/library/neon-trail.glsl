// Shades a stroke as a neon trail: a white-hot core along its middle, a saturated glow that falls off toward its sides, a body that thins and fades toward its tail, pulses of light that run back along it and a bright head, for the trails of fast projectiles and swipes drawn with the additive blend.
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
    float along = clamp(local.x, 0.0, 1.0);

    // The trail is widest at its head and thins toward its tail, so the distance across grows there.
    float thickness = mix(0.25, 1.0, pow(along, 0.7));
    float across = abs(local.y * 2.0 - 1.0) / thickness;
    float core = exp(-pow(across / max(core_width, 0.01), 2.0) * 2.5);

    // The glow reaches nothing at the side of the stroke, so its edge never shows.
    float window = 1.0 - smoothstep(0.6 / thickness, 1.0 / thickness, across);
    float glow = (exp(-across * 4.0) * 1.0 + exp(-across * 1.4) * 0.5) * window;

    // Pulses of light run from the head back toward the tail, and the head burns brighter.
    float pulses = 1.0 - flicker + flicker * (0.55 + 0.45 * sin(along * 38.0 + time * 22.0)) * (0.7 + 0.6 * library_noise(vec2(along * 18.0 + time * 9.0, time * 2.0)));
    float head = 1.0 + exp(-(1.0 - along) * 18.0) * 1.5;
    float tail = pow(along, fade) * smoothstep(1.0, 0.985, along);

    vec3 light = (glow_color.rgb * glow * pulses + vec3(1.0) * core * 1.2) * head * tail;
    float amount = clamp(max(max(light.r, light.g), light.b), 0.0, 1.0);
    haylen_output(vec4(light / max(amount, 0.0001), amount * color.a * glow_color.a));
}
@end

@program neonTrail haylen_vs neonTrail_fs
