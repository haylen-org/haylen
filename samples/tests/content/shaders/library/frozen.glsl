// Covers a sprite with ice: a cold tint, frost from noise and sparkles that come and go.
@include haylen/material.glsl
@include common.glsl

@fs frozen_fs
@include_block haylen_fragment
@include_block library_common
layout(binding=1) uniform frozen_params {
    float amount;
    float time;
};

void main() {
    vec4 base = haylen_base(uv);
    vec2 local = library_local(uv);
    float frost = library_fbm(local * 12.0);
    vec3 ice = mix(vec3(0.55, 0.8, 1.0) * (library_luminance(base.rgb) * 0.6 + 0.5), vec3(1.0), smoothstep(0.55, 0.75, frost));
    float sparkle = step(0.985, library_hash(floor(local * 60.0) + floor(time * 4.0)));
    haylen_output(vec4(mix(base.rgb, ice + sparkle, amount), base.a));
}
@end

@program frozen haylen_vs frozen_fs
