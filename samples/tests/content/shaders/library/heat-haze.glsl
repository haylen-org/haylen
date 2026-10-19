// Shimmers an image like air over hot ground.
@include haylen/material.glsl
@include common.glsl

@fs heatHaze_fs
@include_block haylen_fragment
@include_block library_common
layout(binding=1) uniform heatHaze_params {
    float amount;
    float speed;
    float time;
};

void main() {
    vec2 local = library_local(uv);
    float shimmer = library_noise(vec2(local.x * 12.0, local.y * 8.0 - time * speed)) - 0.5;
    haylen_output(haylen_base(haylen_inside(library_texture_point(local + vec2(shimmer * amount * 0.02, shimmer * amount * 0.01)))));
}
@end

@program heatHaze haylen_vs heatHaze_fs
