// A glass effect of the test project: a lens that magnifies what lies behind it around its center, most in the middle and not at all at the rim, with a darker edge and a soft spot of light at the top left.
@include haylen/material.glsl

@fs glass_lens_fs
@include_block haylen_fragment
layout(binding=1) uniform glass_lens_params {
    vec2 center;
    float radius;
    float zoom;
};

void main() {
#ifdef HAYLEN_SHAPE
    vec2 away = haylen_output_point - center;
    float reach = clamp(length(away) / max(radius, 0.0001), 0.0, 1.0);
    float power = mix(zoom, 1.0, reach * reach);
    vec4 behind = haylen_backdrop(-away * (1.0 - 1.0 / power));
    vec3 seen = behind.rgb / max(behind.a, 0.0001);
    seen *= 1.0 - 0.35 * smoothstep(0.75, 1.0, reach);
    float spot = 1.0 - smoothstep(0.0, 0.35, length(away / max(radius, 0.0001) + vec2(0.35, 0.4)));
    seen = mix(seen, vec3(1.0), spot * 0.35);
    haylen_output(vec4(seen, 1.0) * color);
#else
    haylen_output(haylen_base(uv));
#endif
}
@end

@program glass_lens haylen_vs glass_lens_fs
