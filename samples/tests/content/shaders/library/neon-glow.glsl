// Turns a shape into a neon light: a white core and a halo of the glow color around it that pulses, for bullets, arrows and pickups drawn with the additive blend.
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
    vec4 base = haylen_base(uv);
    float halo = 0.0;
    for (int ring = 1; ring <= 5; ring++) {
        float distance = radius * float(ring) / 5.0;
        float weight = 1.0 - float(ring - 1) / 5.0;
        for (int index = 0; index < 16; index++) {
            float angle = (float(index) + float(ring) * 0.5) * 0.3926991;
            halo += haylen_texture(haylen_inside(uv + vec2(cos(angle), sin(angle)) * texel * distance)).a * weight;
        }
    }
    halo /= 48.0;
    float beat = 1.0 + pulse * sin(time * 6.0);
    vec3 core = mix(glow_color.rgb, vec3(1.0), base.a * 0.85);
    vec3 shade = mix(glow_color.rgb * beat, core, base.a);
    haylen_output(vec4(shade, clamp(base.a + halo * intensity * beat, 0.0, 1.0) * glow_color.a));
}
@end

@program neonGlow haylen_vs neonGlow_fs
