// Moves the red and blue channels of an image apart by a count of pixels, like a cheap lens or a hit.
@include haylen/material.glsl
@include common.glsl

@fs chromaticSplit_fs
@include_block haylen_fragment
@include_block library_common
layout(binding=1) uniform chromaticSplit_params {
    vec2 texel;
    float offset;
};

void main() {
    vec4 base = haylen_base(uv);
    float red = haylen_texture(haylen_inside(uv + vec2(texel.x * offset, 0.0))).r;
    float blue = haylen_texture(haylen_inside(uv - vec2(texel.x * offset, 0.0))).b;
    haylen_output(vec4(red, base.g, blue, base.a));
}
@end

@program chromaticSplit haylen_vs chromaticSplit_fs
