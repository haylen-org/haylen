// Melts a sprite down in drips of different lengths as the progress grows.
@include haylen/material.glsl
@include common.glsl

@fs melt_fs
@include_block haylen_fragment
@include_block library_common
layout(binding=1) uniform melt_params {
    float progress;
};

void main() {
    vec2 local = library_local(uv);
    float drip = library_noise(vec2(local.x * 14.0, 0.0)) * 0.6 + 0.4;
    vec2 moved = vec2(local.x, local.y - progress * drip);
    vec4 base = haylen_base(haylen_inside(library_texture_point(moved)));
    haylen_output(vec4(base.rgb, base.a * step(0.0, moved.y)));
}
@end

@program melt haylen_vs melt_fs
