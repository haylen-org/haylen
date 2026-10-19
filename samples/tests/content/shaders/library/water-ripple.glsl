// Bends an image with rings that spread from its middle, with bright crests, like a drop on water.
@include haylen/material.glsl
@include common.glsl

@fs waterRipple_fs
@include_block haylen_fragment
@include_block library_common
layout(binding=1) uniform waterRipple_params {
    float amplitude;
    float frequency;
    float speed;
    float time;
};

void main() {
    vec2 local = library_local(uv);
    float distance = length(local - 0.5);
    vec2 direction = (local - 0.5) / max(distance, 0.0001);
    float wave = sin(distance * frequency - time * speed);
    vec2 moved = local + direction * wave * amplitude * (1.0 - distance);
    vec4 base = haylen_base(haylen_inside(library_texture_point(moved)));
    haylen_output(vec4(base.rgb + pow(max(wave, 0.0), 8.0) * 0.25, base.a));
}
@end

@program waterRipple haylen_vs waterRipple_fs
