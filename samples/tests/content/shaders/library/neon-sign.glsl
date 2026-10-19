// Keeps only a glowing tube along the edges of a sprite, like a neon sign, which flickers now and then.
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
    float low = 1.0;
    float high = 0.0;
    for (int index = 0; index < 16; index++) {
        float angle = float(index) * 0.3926991;
        float alpha = haylen_texture(haylen_inside(uv + vec2(cos(angle), sin(angle)) * texel * thickness)).a;
        low = min(low, alpha);
        high = max(high, alpha);
    }
    float tube = high * (1.0 - low);
    float flicker = 0.75 + 0.25 * step(0.08, library_noise(vec2(floor(time * 20.0), 3.0)));
    haylen_output(vec4(mix(neon_color.rgb, vec3(1.0), tube * 0.5) * flicker, tube * neon_color.a * flicker));
}
@end

@program neonSign haylen_vs neonSign_fs
