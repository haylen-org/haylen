// Squashes and stretches a sprite from its feet like jelly, for idle creatures and bounces.
@include haylen/material.glsl
@include common.glsl

@fs jelly_fs
@include_block haylen_fragment
@include_block library_common
layout(binding=1) uniform jelly_params {
    float amount;
    float time;
};

void main() {
    vec2 local = library_local(uv);
    float squash = sin(time * 8.0) * amount;
    float height = 1.0 - local.y;
    vec2 moved = vec2(0.5 + (local.x - 0.5) / (1.0 + squash * height), 1.0 - height / (1.0 - squash * 0.5));
    vec4 base = haylen_base(haylen_inside(library_texture_point(moved)));
    float inside = step(0.0, moved.x) * step(moved.x, 1.0) * step(0.0, moved.y);
    haylen_output(vec4(base.rgb, base.a * inside));
}
@end

@program jelly haylen_vs jelly_fs
