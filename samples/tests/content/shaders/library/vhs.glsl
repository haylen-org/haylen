// Shows an image as an old video tape: rows that wobble, colors that bleed and lines of noise.
@include haylen/material.glsl
@include common.glsl

@fs vhs_fs
@include_block haylen_fragment
@include_block library_common
layout(binding=1) uniform vhs_params {
    float amount;
    float time;
};

void main() {
    vec2 local = library_local(uv);
    float wobble = sin(local.y * 30.0 + time * 2.0) * 0.003 * amount + (library_noise(vec2(time * 5.0, local.y * 80.0)) - 0.5) * 0.004 * amount;
    vec2 moved = local + vec2(wobble, 0.0);
    vec4 base = haylen_base(haylen_inside(library_texture_point(moved)));
    float red = haylen_texture(haylen_inside(library_texture_point(moved + vec2(0.004 * amount, 0.0)))).r;
    float line = step(0.995, library_hash(vec2(floor(local.y * 240.0), floor(time * 30.0)))) * 0.4;
    vec3 shade = vec3(red, base.g, base.b) * 0.95 + line;
    haylen_output(vec4(mix(shade, vec3(library_luminance(shade)), 0.2), base.a));
}
@end

@program vhs haylen_vs vhs_fs
