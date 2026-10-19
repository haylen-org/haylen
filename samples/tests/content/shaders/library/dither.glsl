// Reduces the colors to a few levels with an ordered dither pattern, for the look of old screens.
@include haylen/material.glsl
@include common.glsl

@fs dither_fs
@include_block haylen_fragment
@include_block library_common
layout(binding=1) uniform dither_params {
    float levels;
    float scale;
};

void main() {
    vec4 base = haylen_base(uv);
    vec2 pixel = floor(gl_FragCoord.xy / scale);
    vec2 half_pixel = floor(pixel * 0.5);
    float small = fract(pixel.x * 0.5 + pixel.y * pixel.y * 0.75);
    float large = fract(half_pixel.x * 0.5 + half_pixel.y * half_pixel.y * 0.75);
    float threshold = small + large * 0.25;
    haylen_output(vec4(floor(base.rgb * levels + threshold) / levels, base.a));
}
@end

@program dither haylen_vs dither_fs
