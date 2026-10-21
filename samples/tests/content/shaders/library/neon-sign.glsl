// Turns the outline of a sprite into a neon sign: a thin glass tube along its edge with a nearly white core, a halo of the neon color that glows on both sides of the tube, and a buzz that dims it and an occasional flicker, drawn with the additive blend.
@include haylen/material.glsl
@include common.glsl

@fs neonSign_fs
@include_block haylen_fragment
@include_block library_common
layout(binding=1) uniform neonSign_params {
    vec4 neon_color;
    vec2 texel;
    float thickness;
    float time;
};

void main() {
    // The edge of the sprite lies where its blurred coverage crosses one half, so the tube follows it smoothly.
    float edge = library_glow(uv, texel, thickness * 2.0);
    float tube = exp(-pow((edge - 0.5) / 0.12, 2.0));
    float core = exp(-pow((edge - 0.5) / 0.05, 2.0));

    // The halo fades away from the outline on both sides.
    float spread = library_glow(uv, texel, thickness * 9.0);
    float halo = pow(4.0 * spread * (1.0 - spread), 1.6) * 0.75;

    // The sign buzzes a little all the time and now and then flickers off for a moment.
    float moment = floor(time * 9.0);
    float flicker = step(0.93, library_hash(vec2(moment, 5.0))) * step(fract(time * 9.0), 0.6);
    float power = (0.9 + 0.1 * sin(time * 120.0)) * (1.0 - flicker * 0.75);

    vec3 light = (neon_color.rgb * (tube * 1.3 + halo) + vec3(1.0) * core * 0.9) * power;
    float amount = clamp(max(max(light.r, light.g), light.b), 0.0, 1.0);
    haylen_output(vec4(light / max(amount, 0.0001), amount * neon_color.a));
}
@end

@program neonSign haylen_vs neonSign_fs
