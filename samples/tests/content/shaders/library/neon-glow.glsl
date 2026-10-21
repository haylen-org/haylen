// Turns a shape into a neon light: a white-hot core, a saturated rim and a soft halo of the glow color that spreads around it like the bloom of a bright light and pulses, for bullets, arrows and pickups drawn with the additive blend.
@include haylen/material.glsl
@include common.glsl

@fs neonGlow_fs
@include_block haylen_fragment
@include_block library_common
layout(binding=1) uniform neonGlow_params {
    vec4 glow_color;
    vec2 texel;
    float radius;
    float intensity;
    float pulse;
    float time;
};

void main() {
    float shape = haylen_base(uv).a;
    float beat = 1.0 + pulse * sin(time * 7.0);

    // A tight glow hugs the shape and a wide one spreads far, as the bloom of a bright light does.
    float near = library_glow(uv, texel, radius * 0.45);
    float far = library_glow(uv, texel, radius * 1.6);
    float halo = (pow(near, 0.8) * 0.9 + pow(far, 0.6) * 1.5) * intensity * 0.45 * beat;

    // The core burns white where the shape is thickest and keeps the glow color at its edge.
    float heat = smoothstep(0.7, 1.0, near) * shape;
    vec3 light = glow_color.rgb * halo + mix(glow_color.rgb, vec3(1.0), 0.3 + 0.7 * heat) * shape * 1.4;
    float amount = clamp(max(max(light.r, light.g), light.b), 0.0, 1.0);
    haylen_output(vec4(light / max(amount, 0.0001), amount * glow_color.a));
}
@end

@program neonGlow haylen_vs neonGlow_fs
