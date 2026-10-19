// Breaks an image into rows that jump sideways now and then with their colors apart, like a broken signal.
@include haylen/material.glsl
@include common.glsl

@fs glitch_fs
@include_block haylen_fragment
@include_block library_common
layout(binding=1) uniform glitch_params {
    float amount;
    float time;
};

void main() {
    vec2 local = library_local(uv);
    float jump = floor(time * 12.0);
    float row = floor(local.y * 24.0);
    float shift = (library_hash(vec2(row, jump)) - 0.5) * amount * step(0.75, library_hash(vec2(jump, row * 0.37)));
    vec2 moved = vec2(local.x + shift, local.y);
    vec4 base = haylen_base(haylen_inside(library_texture_point(moved)));
    float split = amount * 0.02;
    float red = haylen_texture(haylen_inside(library_texture_point(moved + vec2(split, 0.0)))).r;
    float blue = haylen_texture(haylen_inside(library_texture_point(moved - vec2(split, 0.0)))).b;
    haylen_output(vec4(red, base.g, blue, base.a));
}
@end

@program glitch haylen_vs glitch_fs
