// Draws a soft shadow of a sprite under it at an offset in pixels, which needs a transparent margin in its image.
@include haylen/material.glsl
@include common.glsl

@fs dropShadow_fs
@include_block haylen_fragment
@include_block library_common
layout(binding=1) uniform dropShadow_params {
    vec4 shadow_color;
    vec2 texel;
    vec2 offset;
    float softness;
};

void main() {
    vec4 base = haylen_base(uv);
    float shadow = 0.0;
    for (int y = -1; y <= 1; y++) {
        for (int x = -1; x <= 1; x++) {
            shadow += haylen_texture(haylen_inside(uv - offset * texel + vec2(float(x), float(y)) * texel * softness)).a;
        }
    }
    shadow = shadow / 9.0 * shadow_color.a * (1.0 - base.a);
    float alpha = base.a + shadow;
    haylen_output(vec4((base.rgb * base.a + shadow_color.rgb * shadow) / max(alpha, 0.0001), alpha));
}
@end

@program dropShadow haylen_vs dropShadow_fs
