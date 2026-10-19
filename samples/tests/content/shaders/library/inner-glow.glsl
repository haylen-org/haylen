// Lights the inside of the edges of a sprite with a color, for selected and powered-up characters.
@include haylen/material.glsl
@include common.glsl

@fs innerGlow_fs
@include_block haylen_fragment
@include_block library_common
layout(binding=1) uniform innerGlow_params {
    vec4 glow_color;
    vec2 texel;
    float radius;
};

void main() {
    vec4 base = haylen_base(uv);
    float low = 1.0;
    for (int index = 0; index < 12; index++) {
        float angle = float(index) * 0.5235988;
        low = min(low, haylen_texture(haylen_inside(uv + vec2(cos(angle), sin(angle)) * texel * radius)).a);
    }
    haylen_output(vec4(mix(base.rgb, glow_color.rgb, base.a * (1.0 - low) * glow_color.a), base.a));
}
@end

@program innerGlow haylen_vs innerGlow_fs
