// Raises an image like a stamp in metal, from the difference of its light across a diagonal.
@include haylen/material.glsl
@include common.glsl

@fs emboss_fs
@include_block haylen_fragment
@include_block library_common
layout(binding=1) uniform emboss_params {
    vec2 texel;
    float strength;
};

void main() {
    vec4 base = haylen_base(uv);
    float before = library_luminance(haylen_texture(haylen_inside(uv - texel)).rgb);
    float after = library_luminance(haylen_texture(haylen_inside(uv + texel)).rgb);
    haylen_output(vec4(vec3(0.5 + (before - after) * strength), base.a));
}
@end

@program emboss haylen_vs emboss_fs
