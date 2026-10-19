// Twists an image around its middle, more toward the center, like a portal or a whirlpool.
@include haylen/material.glsl
@include common.glsl

@fs swirl_fs
@include_block haylen_fragment
@include_block library_common
layout(binding=1) uniform swirl_params {
    float strength;
    float radius;
    float time;
};

void main() {
    vec2 local = library_local(uv) - 0.5;
    float distance = length(local);
    float angle = strength * max(1.0 - distance / radius, 0.0) * sin(time);
    vec2 turned = vec2(local.x * cos(angle) - local.y * sin(angle), local.x * sin(angle) + local.y * cos(angle)) + 0.5;
    haylen_output(haylen_base(haylen_inside(library_texture_point(turned))));
}
@end

@program swirl haylen_vs swirl_fs
