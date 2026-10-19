// Reduces every channel to a few levels, for a flat look of cartoons and posters.
@include haylen/material.glsl
@include common.glsl

@fs posterize_fs
@include_block haylen_fragment
@include_block library_common
layout(binding=1) uniform posterize_params {
    float levels;
};

void main() {
    vec4 base = haylen_base(uv);
    haylen_output(vec4(floor(base.rgb * levels + 0.5) / levels, base.a));
}
@end

@program posterize haylen_vs posterize_fs
