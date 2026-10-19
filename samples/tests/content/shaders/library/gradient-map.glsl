// Maps the light of a sprite onto a gradient between two colors, for team colors and moods.
@include haylen/material.glsl
@include common.glsl

@fs gradientMap_fs
@include_block haylen_fragment
@include_block library_common
layout(binding=1) uniform gradientMap_params {
    vec4 dark_color;
    vec4 light_color;
    float amount;
};

void main() {
    vec4 base = haylen_base(uv);
    haylen_output(vec4(mix(base.rgb, mix(dark_color.rgb, light_color.rgb, library_luminance(base.rgb)), amount), base.a));
}
@end

@program gradientMap haylen_vs gradientMap_fs
