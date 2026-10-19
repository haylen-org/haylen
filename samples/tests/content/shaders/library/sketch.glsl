// Draws the edges of an image as pencil lines on paper.
@include haylen/material.glsl
@include common.glsl

@fs sketch_fs
@include_block haylen_fragment
@include_block library_common
layout(binding=1) uniform sketch_params {
    vec2 texel;
    float strength;
};

void main() {
    float top_left = library_luminance(haylen_texture(haylen_inside(uv + texel * vec2(-1.0, -1.0))).rgb);
    float top = library_luminance(haylen_texture(haylen_inside(uv + texel * vec2(0.0, -1.0))).rgb);
    float top_right = library_luminance(haylen_texture(haylen_inside(uv + texel * vec2(1.0, -1.0))).rgb);
    float left = library_luminance(haylen_texture(haylen_inside(uv + texel * vec2(-1.0, 0.0))).rgb);
    float right = library_luminance(haylen_texture(haylen_inside(uv + texel * vec2(1.0, 0.0))).rgb);
    float bottom_left = library_luminance(haylen_texture(haylen_inside(uv + texel * vec2(-1.0, 1.0))).rgb);
    float bottom = library_luminance(haylen_texture(haylen_inside(uv + texel * vec2(0.0, 1.0))).rgb);
    float bottom_right = library_luminance(haylen_texture(haylen_inside(uv + texel * vec2(1.0, 1.0))).rgb);
    float horizontal = top_left + 2.0 * left + bottom_left - top_right - 2.0 * right - bottom_right;
    float vertical = top_left + 2.0 * top + top_right - bottom_left - 2.0 * bottom - bottom_right;
    float edge = clamp(length(vec2(horizontal, vertical)) * strength, 0.0, 1.0);
    haylen_output(vec4(mix(vec3(0.97, 0.96, 0.92), vec3(0.15), edge), haylen_base(uv).a));
}
@end

@program sketch haylen_vs sketch_fs
